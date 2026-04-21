import json
import time
import paho.mqtt.client as mqtt
from flask import jsonify

class MQTTManager:
    def __init__(self, broker, port, username='', password='', topic_prefix='RK3588/camera'):
        self.broker = broker
        self.port = port
        self.username = username
        self.password = password
        self.topic_prefix = topic_prefix
        self.client = None
        self.connected = False
        self.latest_jetson_info = None
        self.last_info_time = 0
        # 存储多个设备的最新心跳数据: { device_id: {info: {...}, last_seen: timestamp} }
        self.devices = {}
    
    def connect(self):
        """连接MQTT服务器"""
        self.client = mqtt.Client()
        if self.username and self.password:
            self.client.username_pw_set(self.username, self.password)
        
        self.client.on_connect = self._on_connect
        self.client.on_disconnect = self._on_disconnect
        self.client.on_message = self._on_message
        
        try:
            self.client.connect(self.broker, self.port, 60)
            self.client.loop_start()
            return True
        except Exception as e:
            print(f"MQTT连接失败: {e}")
            return False
    
    def _on_connect(self, client, userdata, flags, rc):
        if rc == 0:
            self.connected = True
            print("MQTT连接成功")
            # 订阅设备信息主题
            client.subscribe("rk3588lyh/info")
        else:
            self.connected = False
            print(f"MQTT连接失败, 返回码: {rc}")

    def _on_message(self, client, userdata, msg):
        if msg.topic == "rk3588lyh/info":
            try:
                payload = json.loads(msg.payload.decode('utf-8'))
                device_id = payload.get('device_id', 'unknown')
                camera_info = payload.get('camera', {})
                
                normalized_data = {
                    'device_id': device_id,
                    'camera': camera_info,
                    'last_seen': time.time()
                }
                
                self.devices[device_id] = {
                    'info': normalized_data,
                    'last_seen': time.time(),
                    'raw_payload': payload
                }
                
                # 兼容旧逻辑
                self.latest_jetson_info = normalized_data
                self.last_info_time = time.time()
                self.connected = True  # 收到消息说明连接肯定正常
                
                # print(f"收到设备心跳: {device_id}, 摄像头: {camera_info.get('id', 'unknown')} @ {camera_info.get('location', 'unknown')}")
            except Exception as e:
                print(f"解析 rk3588lyh/info 消息失败: {e}")
            
    def _on_disconnect(self, client, userdata, rc):
        self.connected = False
        print(f"MQTT断开连接, 返回码: {rc}")
        if rc != 0:
            print("尝试自动重连...")
            # loop_start 会处理自动重连，我们只需要更新状态
    
    def check_connection(self):
        """主动检查连接状态"""
        if self.client:
            self.connected = self.client.is_connected()
        return self.connected
    
    def publish(self, topic, payload):
        """发布消息"""
        if not self.connected or not self.client:
            return False, "MQTT未连接"
        
        try:
            full_topic = f"{self.topic_prefix}/{topic}"
            result = self.client.publish(full_topic, json.dumps(payload),qos=2)
            if result.rc == mqtt.MQTT_ERR_SUCCESS:
                return True, "消息发送成功"
            else:
                return False, "消息发送失败"
        except Exception as e:
            return False, str(e)
    
    def send_camera_command(self, device_id, camera_id, command):
        if isinstance(command, dict):
            command['device'] = device_id
            command['camera_id'] = camera_id
        return self.publish("command", command)

    def publish_ptz(self, device_id, camera_id, action, step=None):
        """发布 PTZ 指令到 MQTT

        payload 结构示例:
        {
            "type": "ptz",
            "action": "up|down|left|right|center",
            "step": number,        # 可选
            "device": <device_id>,
            "camera_id": <camera_id>,
            "timestamp": <int>
        }
        """
        # 校验动作是否合法
        if action not in ("up", "down", "left", "right", "center"):
            return False, "无效的 PTZ 动作"

        payload = {
            'type': 'ptz',
            'action': action,
            'device': device_id,
            'camera_id': camera_id,
            'timestamp': int(time.time())
        }

        if step is not None:
            try:
                payload['step'] = int(step)
            except Exception:
                return False, 'step 必须为整数'
        
        return self.publish("command", payload)

    def disconnect(self):
        """断开连接"""
        if self.client:
            self.client.loop_stop()
            self.client.disconnect()

    def get_active_cameras(self):
        """获取当前在线的摄像头列表"""
        if not self.connected or not self.latest_jetson_info:
            return []
        
        if (time.time() - self.last_info_time) > 10:
            return []
        
        camera = self.latest_jetson_info.get('camera')
        if camera:
            return [camera]
        return []
    
    def get_active_data(self):
        """获取所有在线设备的数据统计"""
        now = time.time()
        active_devices = []
        total_cameras = 0
        all_cameras = []
        
        expired_devices = []
        for device_id, data in list(self.devices.items()):
            if now - data['last_seen'] < 10:
                active_devices.append(device_id)
                camera = data['info'].get('camera')
                if camera:
                    total_cameras += 1
                    camera['device_id'] = device_id
                    all_cameras.append(camera)
            else:
                expired_devices.append(device_id)
        
        for dev_id in expired_devices:
            if now - self.devices[dev_id]['last_seen'] > 60:
                del self.devices[dev_id]

        return {
            'device_count': len(active_devices),
            'camera_count': total_cameras,
            'cameras': all_cameras,
            'devices': active_devices,
            'device_details': {dev_id: self.devices[dev_id]['info'] for dev_id in active_devices}
        }


mqtt_manager = None

def init_mqtt(app):
    """初始化MQTT（不自动连接，等待用户手动连接）"""
    global mqtt_manager
    mqtt_manager = MQTTManager(
        broker='',
        port=1883,
        username='',
        password='',
        topic_prefix=app.config.get('MQTT_TOPIC_PREFIX', 'rk3588lyh/camera')
    )
    return mqtt_manager

def get_mqtt_status():
    """获取MQTT连接状态"""
    if mqtt_manager:
        return {
            'connected': mqtt_manager.connected,
            'broker': mqtt_manager.broker,
            'port': mqtt_manager.port
        }
    return {'connected': False}

def send_camera_command(device_id, camera_id, command):
    """发送摄像头命令"""
    if mqtt_manager:
        return mqtt_manager.send_camera_command(device_id, camera_id, command)
    return False, "MQTT未初始化"
