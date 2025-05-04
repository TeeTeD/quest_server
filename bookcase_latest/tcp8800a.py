import asyncio
import time
from datetime import datetime, timedelta

devices = {}
devices_lock = asyncio.Lock()
packet_queue = asyncio.Queue()
HEARTBEAT_TIMEOUT = timedelta(seconds=10)

SLAVE_BOOKCASE_CONF = {
    "CONF041;",
    "CONF051;",
    "CONF321;",
    "CONF262;",
    "LC000200742550;"
}

SLAVE_TEST_CONF = {
    "CONF231;",
    "CONF221;",
    "CONF271;",
    "CONF022;",
    "LC001700742000;"
}

SLAVE_CONF_LIST = {
    "boca": SLAVE_BOOKCASE_CONF,
    "efgh": SLAVE_TEST_CONF
}

def RSST():
    command = "RSST;"
    return command

def SETD(pin, state):
    command = "SETD"   
    command += f"{pin:02d}"
    command +=f"{state:01d}"
    command += ";" 
    return command

def LC01(strip_number):
    command = "LC01"
    command += str(strip_number)
    command += ";" 
    return command

def LC02(strip_number, pixel_number, red, green, blue):
    command = "LC02"
    command += str(strip_number)    
    command += f"{pixel_number:04d}"
    command += f"{red:03d}"
    command += f"{green:03d}"
    command += f"{blue:03d}"
    command += ";" 
    return command

def LC03(strip_number, pixel_number, amount, red, green, blue):
    command = "LC03"
    command += str(strip_number)    
    command += f"{pixel_number:04d}"
    command += f"{amount:04d}"
    command += f"{red:03d}"
    command += f"{green:03d}"
    command += f"{blue:03d}"
    command += ";" 
    return command

def LC12(strip_number, pixel_number, red, green, blue, time):
    command = "LC12"
    command += str(strip_number)    
    command += f"{pixel_number:04d}"
    command += f"{red:03d}"
    command += f"{green:03d}"
    command += f"{blue:03d}"
    command += f"{time:05d}"
    command += ";" 
    return command

def LC13(strip_number, pixel_number, amount, red, green, blue, time):
    command = "LC13"
    command += str(strip_number)    
    command += f"{pixel_number:04d}"
    command += f"{amount:04d}"
    command += f"{red:03d}"
    command += f"{green:03d}"
    command += f"{blue:03d}"
    command += f"{time:05d}"
    command += ";" 
    return command


async def handle_client(reader, writer):
    client_address = writer.get_extra_info('peername')
    print(f"Подключено устройство с адресом: {client_address}")
    device_id = None
    buffer = ""

    try:
        device_id = (await reader.read(1024)).decode('utf-8').strip()
        print(f"Устройство {device_id} подключилось с IP {client_address[0]}")

        async with devices_lock:
            task = asyncio.current_task()
            devices[device_id] = (client_address[0], writer, reader, datetime.now(), task)

        if device_id in SLAVE_CONF_LIST:
            config = SLAVE_CONF_LIST[device_id]
            print(f"Попытка отправить конфигурацию на устройство {device_id}...")
            await send_configuration(device_id, config)

        while True:
            try:
                data = await asyncio.wait_for(reader.read(1024), timeout=HEARTBEAT_TIMEOUT.total_seconds())
                if not data:
                    break

                buffer += data.decode('utf-8')
                packets = buffer.split(";")
                for packet in packets[:-1]:
                    if packet:
                        print(f"Получен пакет от устройства {device_id}: {packet}")
                        await packet_queue.put((device_id, packet))

                buffer = packets[-1]

                async with devices_lock:
                    devices[device_id] = (client_address[0], writer, reader, datetime.now(), task)

            except asyncio.TimeoutError:
                print(f"Устройство {device_id} неактивно более {HEARTBEAT_TIMEOUT}")
                break

    except Exception as e:
        print(f"Ошибка при работе с устройством {client_address}: {e}")
    finally:
        if device_id:
            async with devices_lock:
                if device_id in devices:
                    task = devices[device_id][4]
                    task.cancel()
                    try:
                        await task
                    except asyncio.CancelledError:
                        print(f"Задача для устройства {device_id} отменена.")
                    del devices[device_id]
            writer.close()
            await writer.wait_closed()
            print(f"Устройство {device_id} отключилось")


async def check_timeouts():
    while True:
        async with devices_lock:
            now = datetime.now()
            for device_id in list(devices.keys()):
                ip, writer, reader, last_active, task = devices[device_id]
                if now - last_active > HEARTBEAT_TIMEOUT:
                    print(f"Устройство {device_id} отключилось по таймауту")
                    task.cancel()
                    try:
                        await task
                    except asyncio.CancelledError:
                        print(f"Задача для устройства {device_id} отменена.")
                    del devices[device_id]
        await asyncio.sleep(5)


async def send_configuration(device_id, config):
    async with devices_lock:
        if device_id not in devices:
            print(f"Устройство {device_id} не подключено.")
            return False

        ip, writer, reader, last_active, task = devices[device_id]

    for command in config:
        writer.write(command.encode('utf-8'))
        await writer.drain()
        print(f"Отправлено на устройство {device_id}: {command}")

    return True


async def bookcase_handler():
    book_1_press = False
    book_2_press = False
    book_3_press = False

    book_1_flag = False
    book_2_flag = False
    book_3_flag = False

    already_wrong = False

    while True:
        device_id, packet = await packet_queue.get()

        async with devices_lock:
            if device_id in devices:
                ip, writer, reader, last_active, task = devices[device_id]

                try:#64, 44, 13

                    if "GETD221" in packet:
                        book_1_press = True
                        if not book_2_flag and not book_3_flag and not already_wrong:
                            book_1_flag = True
                            writer.write(LC13(0, 60, 6, 0, 127, 0, 500).encode('utf-8'))
                            await writer.drain()
                        else:
                            already_wrong = True
                            writer.write(LC13(0, 60, 6, 255, 0, 0, 500).encode('utf-8'))
                            await writer.drain()

                    
                    if "GETD231" in packet:
                        book_2_press = True
                        if book_1_flag and not book_3_flag and not already_wrong:
                            book_2_flag = True
                            writer.write(LC13(0, 41, 6, 0, 127, 0, 500).encode('utf-8'))
                            await writer.drain()
                        else:
                            already_wrong = True
                            writer.write(LC13(0, 41, 6, 255, 0, 0, 500).encode('utf-8'))
                            await writer.drain()


                    if "GETD271" in packet:
                        book_3_press = True
                        if book_1_flag and book_2_flag and not already_wrong:
                            book_3_flag = True
                            writer.write(LC13(0, 0, 74, 0, 255, 0, 5000).encode('utf-8'))
                            time.sleep(5)
                            writer.write(SETD(4, True).encode('utf-8'))
                            await writer.drain()
                        else:
                            already_wrong = True
                            writer.write(LC13(0, 9, 6, 255, 0, 0, 500).encode('utf-8'))
                            await writer.drain()
                        

                    if "GETD220" in packet:
                        book_1_press = False
                        book_1_flag = False
                        writer.write(LC13(0, 60, 6, 0, 0, 0, 500).encode('utf-8'))
                        await writer.drain()

                    if "GETD230" in packet:
                        book_2_press = False
                        book_2_flag = False
                        writer.write(LC13(0, 41, 6, 0, 0, 0, 500).encode('utf-8'))
                        await writer.drain()

                    if "GETD270" in packet:
                        book_3_press = False
                        book_3_flag = False
                        writer.write(LC13(0, 9, 6, 0, 0, 0, 500).encode('utf-8'))
                        await writer.drain()
                        
                    if not book_1_press and not book_2_press and not book_3_press:
                        already_wrong = False
                        writer.write(LC03(0, 0, 74, 0, 0, 0).encode('utf-8'))
                        writer.write(LC01(0).encode('utf-8'))
                        writer.write(SETD(4, False).encode('utf-8'))

                            
                except ConnectionResetError:
                    print(f"Соединение с устройством {device_id} потеряно.")
                    async with devices_lock:
                        if device_id in devices:
                            task = devices[device_id][4]
                            task.cancel()
                            try:
                                await task
                            except asyncio.CancelledError:
                                print(f"Задача для устройства {device_id} отменена.")
                            del devices[device_id]


async def start_server(host='0.0.0.0', port=8800):
    server = await asyncio.start_server(handle_client, host, port)
    addr = server.sockets[0].getsockname()
    print(f"Сервер запущен на {addr}")

    asyncio.create_task(check_timeouts())
    asyncio.create_task(bookcase_handler())

    async with server:
        await server.serve_forever()


if __name__ == "__main__":
    asyncio.run(start_server())
