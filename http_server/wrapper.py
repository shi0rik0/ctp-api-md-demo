from fastapi import FastAPI
from fastapi.responses import StreamingResponse
import asyncio
import sys
import logging
from typing import Set

# 配置日志，便于调试
logging.basicConfig(level=logging.INFO, stream=sys.stderr,
                    format='%(asctime)s - %(levelname)s - %(message)s')
logger = logging.getLogger(__name__)

# 定义一个广播通道类
class BroadcastChannel:
    def __init__(self, max_queue_size: int = 100):
        """
        初始化广播通道。
        :param max_queue_size: 每个订阅者队列的最大容量。如果队列满，消息将被丢弃。
        """
        self._subscribers: Set[asyncio.Queue] = set()
        self._lock = asyncio.Lock() # 用于保护 _subscribers 集合
        self._max_queue_size = max_queue_size
        logger.info(f"BroadcastChannel initialized with max_queue_size={max_queue_size}")

    async def subscribe(self) -> asyncio.Queue:
        """
        订阅广播通道。返回一个专门用于接收消息的异步队列。
        """
        queue = asyncio.Queue(maxsize=self._max_queue_size)
        async with self._lock:
            self._subscribers.add(queue)
        logger.info(f"New subscriber {id(queue)} added. Total subscribers: {len(self._subscribers)}")
        return queue

    async def unsubscribe(self, queue: asyncio.Queue):
        """
        取消订阅广播通道。
        """
        async with self._lock:
            if queue in self._subscribers:
                self._subscribers.remove(queue)
                # 可以选择在这里清空队列，但通常在客户端断开时，队列会被垃圾回收
                logger.info(f"Subscriber {id(queue)} removed. Total subscribers: {len(self._subscribers)}")
            else:
                logger.warning(f"Attempted to remove non-existent subscriber {id(queue)}.")

    async def publish(self, message: str):
        """
        发布消息到所有订阅者。
        如果订阅者的队列已满，消息将被丢弃，以防止慢客户端阻塞发布者。
        """
        if not message.strip(): # 不发布空消息
            return

        # 在获取锁的情况下复制订阅者列表，以避免在迭代时因修改集合而引发错误
        async with self._lock:
            subscribers_to_notify = list(self._subscribers)

        # 非阻塞地将消息放入每个订阅者的队列
        for queue in subscribers_to_notify:
            try:
                queue.put_nowait(message) # 使用 put_nowait 实现非阻塞
            except asyncio.QueueFull:
                logger.warning(f"Subscriber queue {id(queue)} is full, dropping message: '{message[:50]}...'")
            except Exception as e:
                logger.error(f"Error putting message to subscriber queue {id(queue)}: {e}")

app = FastAPI()
# 创建一个全局的广播通道实例
broadcast_channel = BroadcastChannel(max_queue_size=50) # 可以调整队列大小

async def read_input_and_publish():
    """
    异步读取标准输入的SSE格式流，并将解析出的数据发布到广播通道。
    """
    loop = asyncio.get_event_loop()
    logger.info("Starting SSE stdin reader task.")
    buffer = ""
    try:
        while True:
            # 使用 run_in_executor 在单独的线程中执行同步的 readline
            line = await loop.run_in_executor(None, sys.stdin.readline)
            if not line:  # 如果 readline 返回空字符串，表示 stdin 达到 EOF
                logger.info("EOF reached on stdin, stopping input reading.")
                break  # 退出循环，停止读取

            buffer += line
            if buffer.endswith('\n\n'):
                # 解析SSE消息
                event_data = buffer.strip()
                if event_data.startswith('data: '):
                    data = event_data[6:].strip()
                    await broadcast_channel.publish(data)
                buffer = ""  # 清空缓冲区
            
    except Exception as e:
        logger.critical(f"Critical error in read_input_and_publish task: {e}")
    finally:
        logger.info("Stdin reader task stopped.")


@app.on_event("startup")
async def startup_event():
    """
    FastAPI 应用启动时创建读取 stdin 的任务。
    """
    asyncio.create_task(read_input_and_publish())
    logger.info("Application startup: stdin reader task initiated.")

async def event_generator(client_queue: asyncio.Queue):
    """
    为每个连接的客户端生成 SSE 事件流。
    """
    logger.info(f"Event generator started for client {id(client_queue)}.")
    try:
        while True:
            # 从客户端队列获取数据
            line = await client_queue.get()
            
            # 按照 SSE 格式发送数据
            yield f"data: {line}\n\n"
            client_queue.task_done() # 标记任务完成
    except asyncio.CancelledError:
        # 当客户端断开连接时，FastAPI 会取消这个协程
        logger.info(f"Client {id(client_queue)} disconnected (CancelledError).")
    except Exception as e:
        # 捕获其他可能的异常
        logger.error(f"Error in event_generator for client {id(client_queue)}: {e}")
    finally:
        # 确保在协程结束时（无论正常或异常）从广播通道中取消订阅
        await broadcast_channel.unsubscribe(client_queue)


@app.get("/events")
async def sse_endpoint():
    """
    SSE 接口，处理新的客户端连接。
    """
    # 客户端订阅广播通道，获取其专属的接收队列
    client_queue = await broadcast_channel.subscribe()
    
    # 返回 StreamingResponse，使用 event_generator 生成 SSE 事件
    return StreamingResponse(event_generator(client_queue), media_type="text/event-stream")

if __name__ == "__main__":
    import uvicorn
    # 运行 FastAPI 应用
    # 示例用法：
    # 1. 启动应用: python your_app_name.py
    # 2. 在另一个终端向其标准输入发送数据: echo "Hello World" | nc localhost 8000
    #    或者: (sleep 1; echo "Line 1"; sleep 1; echo "Line 2") | python your_app_name.py
    # 3. 在浏览器中访问 http://localhost:8000/events 或使用 curl:
    #    curl -N http://localhost:8000/events
    uvicorn.run(app, host="0.0.0.0", port=8000)