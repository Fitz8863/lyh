from flask_login import login_required, current_user
from flask import Blueprint, render_template, request, jsonify, make_response, abort
from . import db, login_manager
from .models import MqttConfig
from .auth import admin_required

settings_bp = Blueprint('settings', __name__, url_prefix='/settings')

@settings_bp.before_request
def before_request():
    if not current_user.is_authenticated:
        return login_manager.unauthorized()

@settings_bp.route('/')
def index():
    """系统设置页面"""
    return render_template('settings.html')

@settings_bp.route('/api/mqtt/status', methods=['GET'])
def get_mqtt_status():
    """获取MQTT连接状态"""
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if mqtt_manager and mqtt_manager.broker:
            return jsonify({
                'connected': mqtt_manager.connected,
                'broker': mqtt_manager.broker,
                'port': mqtt_manager.port
            }), 200
        return jsonify({'connected': False}), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/mqtt/connect', methods=['POST'])
def mqtt_connect():
    data = request.json
    broker = data.get('broker')
    port = data.get('port', 1883)
    username = data.get('username', '')
    password = data.get('password', '')
    mediamtx_whip_port = data.get('mediamtx_whip_port', 8889)
    mediamtx_rtsp_port = data.get('mediamtx_rtsp_port', 8554)
    save = data.get('save', False)
    
    if not broker:
        return jsonify({'error': '请输入服务器地址'}), 400
    
    try:
        from blueprints.mqtt_manager import MQTTManager
        import blueprints.mqtt_manager as mqtt_module
        
        if mqtt_module.mqtt_manager and mqtt_module.mqtt_manager.client:
            mqtt_module.mqtt_manager.disconnect()
        
        mqtt_module.mqtt_manager = MQTTManager(
            broker=broker,
            port=port,
            username=username,
            password=password,
            topic_prefix='rk3588lyh/camera'
        )
        
        success = mqtt_module.mqtt_manager.connect()
        
        if success:
            if save:
                MqttConfig.query.update({'is_active': False})
                
                new_config = MqttConfig(
                    broker=broker,
                    port=port,
                    username=username,
                    password=password,
                    mediamtx_whip_port=mediamtx_whip_port,
                    mediamtx_rtsp_port=mediamtx_rtsp_port,
                    is_active=True
                )
                db.session.add(new_config)
                db.session.commit()
            
            return jsonify({
                'connected': True,
                'broker': broker,
                'port': port
            }), 200
        else:
            return jsonify({'connected': False, 'error': '连接失败'}), 500
            
    except Exception as e:
        return jsonify({'connected': False, 'error': str(e)}), 500

@settings_bp.route('/api/mqtt/disconnect', methods=['POST'])
def mqtt_disconnect():
    try:
        import blueprints.mqtt_manager as mqtt_module
        if mqtt_module.mqtt_manager and mqtt_module.mqtt_manager.client:
            mqtt_module.mqtt_manager.disconnect()
        
        response = make_response(jsonify({'message': '已断开连接'}), 200)
        response.set_cookie('mqtt_auto_connect', 'false', max_age=30*24*60*60)
        return response
    except Exception as e:
        return jsonify({'error': str(e)}), 500


@settings_bp.route('/api/mqtt/configs', methods=['GET'])
def get_mqtt_configs():
    """获取所有保存的MQTT配置（按broker去重）"""
    # 按broker分组，只取最新的记录
    configs = db.session.query(MqttConfig).order_by(MqttConfig.broker, MqttConfig.created_at.desc()).all()
    
    # 去重，保留每个broker最新的记录
    seen = set()
    unique_configs = []
    for c in configs:
        if c.broker not in seen:
            seen.add(c.broker)
            unique_configs.append(c)
    
    return jsonify({
        'configs': [{
            'id': c.id,
            'broker': c.broker,
            'port': c.port,
            'username': c.username or '',
            'mediamtx_whip_port': c.mediamtx_whip_port or 8889,
            'mediamtx_rtsp_port': c.mediamtx_rtsp_port or 8554,
            'is_active': c.is_active
        } for c in unique_configs]
    }), 200

@settings_bp.route('/api/esp8266/ptz', methods=['POST'])
def esp8266_ptz():
    from flask import current_app
    data = request.json
    row = data.get('row', 0)
    col = data.get('col', 0)
    
    esp_id = current_app.config.get('ESP8266_ID', '002')
    
    try:
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
        
        success, message = mqtt_manager.send_esp8266_command(esp_id, int(row), int(col))
        if success:
            return jsonify({'message': 'ESP8266指令发送成功', 'row': row, 'col': col}), 200
        return jsonify({'error': message}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/mqtt/ptz', methods=['POST'])
def mqtt_ptz():
    # PTZ control: expect device_id, camera_id, action, and optional step
    data = request.json
    device_id = None
    camera_id = None
    action = None
    step = None

    if data:
        device_id = data.get('device_id')
        camera_id = data.get('camera_id')
        action = data.get('action')
        step = data.get('step')

    if not device_id or not camera_id or not action:
        return jsonify({'error': '缺少必要参数'}), 400

    if action not in ('up', 'down', 'left', 'right', 'center'):
        return jsonify({'error': '无效的 PTZ 动作'}), 400

    try:
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
        success, message = mqtt_manager.publish_ptz(device_id, camera_id, action, step)
        if success:
            return jsonify({'message': 'PTZ 指令已发送'}), 200
        return jsonify({'error': message}), 500
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/api/mqtt/realtime-stats', methods=['GET'])
def get_mqtt_realtime_stats():
    """获取MQTT实时设备统计（在线设备数、摄像头数）"""
    try:
        import time
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager:
            return jsonify({
                'connected': False,
                'device_count': 0,
                'camera_count': 0,
                'devices': []
            }), 200
        
        # 获取活跃设备数据
        data = mqtt_manager.get_active_data()
        
        return jsonify({
            'connected': mqtt_manager.connected,
            'device_count': data.get('device_count', 0),
            'camera_count': data.get('camera_count', 0),
            'devices': data.get('devices', []),
            'device_details': data.get('device_details', {})
        }), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500

@settings_bp.route('/apijetson/info', methods=['GET'])
def get_jetson_info():
    """获取最新的Jetson设备信息"""
    try:
        import time
        from blueprints.mqtt_manager import mqtt_manager
        if not mqtt_manager or not mqtt_manager.connected:
            return jsonify({'error': 'MQTT未连接'}), 400
            
        if not mqtt_manager.latest_jetson_info:
            return jsonify({'message': '等待数据中...', 'data': None}), 200
            
        # 检查是否超时 (10秒未收到心跳包视为断开)
        is_online = (time.time() - mqtt_manager.last_info_time) < 10
        
        return jsonify({
            'message': 'success',
            'data': mqtt_manager.latest_jetson_info,
            'is_online': is_online
        }), 200
    except Exception as e:
        return jsonify({'error': str(e)}), 500
