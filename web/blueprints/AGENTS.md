# blueprints/ - 后端业务逻辑层

## 概览
Flask Blueprint 模块集合，承载全部后端路由、数据模型和设备通信逻辑。

## 文件职责

| 文件 | 职责 | 关键导出 |
|------|------|----------|
| `__init__.py` | db/login_manager 初始化，**全局登录拦截** (`check_login`) | `db`, `init_db` |
| `models.py` | 3 个 ORM 模型：`User`, `Capture`, `MqttConfig` | 模型类 |
| `auth.py` | 注册/登录/登出 + 邮箱验证码 | `auth_bp`, `admin_required`, `super_admin_required` |
| `main.py` | 首页/监控/告警页面路由 + 摄像头列表 API | `main_bp` |
| `capture.py` | 抓拍上传（无需登录）、列表/统计/删除 + SocketIO 事件 | `capture_bp` |
| `settings.py` | MQTT 连接管理、对讲控制、PTZ 指令、摄像头配置下发 | `settings_bp` |
| `user_management.py` | 用户 CRUD + 头像上传 + 个人资料 | `user_mgmt_bp` |
| `mqtt_manager.py` | paho-mqtt 客户端封装，设备心跳、多设备管理 | `MQTTManager`, `mqtt_manager`, `init_mqtt` |
| `video_stream.py` | 摄像头列表（已转为纯 MQTT 动态发现） | `list_cameras` |

## 全局登录拦截机制
`__init__.py` 的 `check_login` 作为 `before_request` 拦截所有请求。白名单路由：
- `auth.login`, `auth.register`, `auth.api_send_signup_code`
- `static`（静态文件）
- `capture.upload_capture`（边缘设备上传，无需登录）
- `settings.intercom_webhook_stop`（Webhook 回调）

新增无需登录的路由时**必须在此白名单中添加 endpoint**，否则会被拦截。

## RBAC 装饰器
- `@admin_required` — 允许 `admin` + `assistant`
- `@super_admin_required` — 仅允许 `admin`
- `is_admin` 属性返回 `role in ['admin', 'assistant']`
- `is_super_admin` 属性返回 `role == 'admin'`

注意：`is_admin` 包含 assistant，与直觉不同。

## API 返回格式
两种风格并存（新代码应统一为第二种）：
- capture 模块：`{'code': 200, 'message': '...'}` + HTTP 200
- 其他模块：`{'error': '...'}` 或 `{'message': '...'}` + 对应 HTTP 状态码

## 数据库操作规则
- 所有 `db.session.commit()` 必须 try-except + `db.session.rollback()`
- `db.create_all()` 在 `__init__.py` 中自动执行，不会更新已有表结构
- 修改模型字段后需手动 `ALTER TABLE` 或清库重启

## MQTT 通信
- 使用 `paho-mqtt` 手动管理（非 flask-mqtt 扩展）
- `mqtt_manager` 是模块级全局单例
- 心跳超时 10 秒，设备记录保留 60 秒后清除
- 两种发布方式：`publish()` 加 topic_prefix 前缀；`publish_raw()` 使用原始 topic

## 反模式（禁止）
- 在 `exts.py` 中导入蓝图或模型 → 循环引用
- 在蓝图文件顶层导入 `mqtt_manager` 实例 → 应在函数内 `from blueprints.mqtt_manager import mqtt_manager`
- 新增蓝图不在 `app.py` 中 `register_blueprint` → 路由不生效
