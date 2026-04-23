# VisionGuard AI - 嵌入式智能跌倒检测与远程监控系统

基于 Flask 的嵌入式 AI 视觉跌倒检测系统，支持多路视频流监控、老人跌倒自动抓拍告警、MQTT 远程配置与消息推送、邮件告警通知、以及多设备的统一管理。

---

## 功能特性

- **用户认证系统**：完整的用户注册、登录、会话管理，支持邮箱验证码注册
- **角色权限控制**：admin / assistant / user 三级角色，管理员专属功能隔离
- **实时视频监控**：多设备视频流并发监控，支持 HTTP/RTSP 视频流接入
- **ESP8266 云台遥控**：通过 MQTT 下发 PTZ 指令，控制远程摄像头方向（row/col 范围限制）
- **跌倒检测与告警**：接收 MQTT 跌倒事件，自动发送邮件告警（含抓拍图片附件）
- **抓拍记录管理**：查看和管理所有抓拍记录，支持多选批量删除、图片预览、筛选分页
- **MQTT 物联网集成**：Web 系统与 RK3588 边缘设备双向通信，心跳监测，设备自动发现
- **实时仪表盘**：显示在线设备、摄像头分辨率/帧率、连接状态等实时数据
- **响应式设计**：基于 Bootstrap 5 构建的深色毛玻璃主题 UI，兼容桌面与移动设备
- **Socket.IO 实时推送**：新抓拍事件实时推送到前端，配合声音报警

---

## 技术栈

### 后端
| 技术 | 版本 | 用途 |
|------|------|------|
| Flask | 3.1+ | 核心 Web 框架 |
| MySQL | 5.7+ | 关系型数据库 |
| SQLAlchemy | 2.0 | ORM 框架 |
| Flask-Login | 0.6.3 | 用户会话管理 |
| Flask-Bcrypt | 1.0.1 | 密码加密 |
| Flask-Mail | - | 邮件发送（告警通知） |
| Flask-SocketIO | - | WebSocket 实时推送 |
| paho-mqtt | 2.1.0 | MQTT 物联网通信 |
| Pillow | - | 图片缩略图生成 |

### 前端
| 技术 | 用途 |
|------|------|
| HTML5 + Jinja2 | 页面模板 |
| Bootstrap 5.3 | UI 框架 |
| Font Awesome 6.0 | 图标库 |
| Socket.IO Client | 实时通信 |
| Vanilla JavaScript | 交互逻辑 |

---

## 项目结构

```text
web/
├── app.py                      # Flask 应用主入口
├── config.py                   # 核心配置（数据库、邮箱、MQTT、ESP8266）
├── exts.py                     # Flask 扩展实例（db, mail, socketio）避免循环导入
├── cameras.json                # 摄像头默认静态配置
├── requirements.txt            # Python 依赖
├── blueprints/                 # Flask 蓝图模块
│   ├── __init__.py             # 数据库初始化、LoginManager、全局登录拦截
│   ├── models.py               # 数据模型 (User, Capture, MqttConfig)
│   ├── main.py                 # 主页面路由（首页、监控、告警）
│   ├── auth.py                 # 身份认证（注册/登录/注销/邮箱验证）
│   ├── capture.py              # 抓拍上传/列表/统计/单条删除/批量删除
│   ├── mqtt_manager.py         # MQTT 客户端（连接/订阅/发布/心跳/邮件告警）
│   ├── settings.py             # 系统设置（MQTT 连接管理、PTZ 指令、ESP8266 控制）
│   ├── video_stream.py         # 摄像头列表（MQTT 动态发现）
│   └── user_management.py      # 用户 CRUD 管理
├── templates/                  # Jinja2 视图模板
│   ├── base.html               # 全局基础布局（导航栏、报警系统、SocketIO）
│   ├── index.html              # 管理员首页仪表盘
│   ├── user_index.html         # 普通用户首页
│   ├── login.html              # 登录页
│   ├── register.html           # 注册页（含邮箱验证码）
│   ├── monitor.html            # 实时视频监控 + PTZ 云台控制
│   ├── alerts.html             # 抓拍告警记录（多选批量删除）
│   ├── settings.html           # 系统设置（MQTT 连接、实时仪表盘）
│   └── user_management.html    # 用户管理页面
├── static/                     # 静态资源
│   ├── bootstrap/              # Bootstrap 框架本地缓存
│   ├── css/style.css           # 全局自定义 CSS（深色毛玻璃主题）
│   ├── img/                    # UI 图片资源
│   ├── avatars/                # 用户头像
│   └── captures/               # 抓拍图片存储目录
│       ├── *.jpg               # 原图
│       └── thumbnails/         # 缩略图
└── AGENTS.md                   # AI Agent 开发规范
```

---

## 快速开始

### 1. 环境准备

- Python 3.10+
- MySQL 5.7+
- Conda（推荐）

```bash
git clone <repository-url>
cd web

conda create -n bishe python=3.10
conda activate bishe
```

### 2. 安装依赖

```bash
pip install Flask==3.1.3 Flask-SQLAlchemy==3.1.1 Flask-Login==0.6.3
pip install Flask-Bcrypt==1.0.1 Flask-Mail Flask-SocketIO
pip install PyMySQL==1.1.2 SQLAlchemy==2.0.48
pip install paho-mqtt==2.1.0 Pillow
```

### 3. 配置数据库

创建 MySQL 数据库：
```bash
mysql -u root -p -e "CREATE DATABASE IF NOT EXISTS lyh DEFAULT CHARACTER SET utf8 COLLATE utf8_general_ci;"
```

修改 `config.py` 中的数据库连接参数：
```python
HOSTNAME = '127.0.0.1'
PORT = 3306
USERNAME = 'root'
PASSWORD = 'your_password'   # 替换为你的 MySQL 密码
DATABASE = 'lyh'
```

### 4. 配置邮箱（用于跌倒告警通知）

修改 `config.py` 中的邮箱配置：
```python
MAIL_SERVER = 'smtp.qq.com'
MAIL_USE_SSL = True
MAIL_PORT = 465
MAIL_USERNAME = 'your_email@qq.com'
MAIL_PASSWORD = 'your_smtp_auth_code'
MAIL_DEFAULT_SENDER = 'your_email@qq.com'

TARGET_EMAIL = ['target@example.com']   # 接收告警的邮箱列表
```

### 5. 运行服务

```bash
python app.py
```

服务启动在 `http://0.0.0.0:5002`。数据库表会在首次启动时自动创建。

首次使用需注册账号，然后通过管理员账号登录系统。

---

## 系统架构

### 整体数据流

```text
┌─────────────┐     MQTT 心跳      ┌──────────────┐
│  RK3588     │ ──────────────────→ │              │
│  边缘设备   │     rk3588lyh/info  │              │
│             │                     │   Flask      │
│  (跌倒检测  │     MQTT 告警       │   Web 服务   │
│   + 摄像头) │ ──────────────────→ │              │
│             │  rk3588lyh/001/report│             │
│             │                     │              │
│             │     HTTP 上传图片    │              │
│             │ ──────────────────→ │  ──→ 邮件告警│
│             │   POST /capture/upload│  ──→ 数据库 │
│             │                     │  ──→ SocketIO│
│             │                     │      实时推送 │
│             │     MQTT PTZ 指令    │              │
│             │ ←────────────────── │              │
│  ESP8266    │  rk3588lyh/esp8266/cmd│             │
│  (云台控制) │                     └──────────────┘
└─────────────┘                            │
                                           ▼
                                    ┌──────────────┐
                                    │   浏览器      │
                                    │  (前端页面)   │
                                    └──────────────┘
```

### MQTT 主题说明

| 主题 | 方向 | 用途 | 数据格式 |
|------|------|------|----------|
| `rk3588lyh/info` | 设备 → Web | 设备心跳包 | `{"device_id":"rk3588lyh","camera":{"id":"001","location":"校园大门","http_url":"...","rtsp_url":"...","resolution":{"width":1920,"height":1080,"fps":25.8}}}` |
| `rk3588lyh/001/report` | 设备 → Web | 跌倒检测告警 | `{"device_id":"rk3588lyh","camera_id":"001","detection":"falldown","count":5}` |
| `rk3588lyh/esp8266/cmd` | Web → 设备 | ESP8266 云台控制 | `{"id":"002","row":3,"col":5}` |

### 心跳超时机制

- 设备心跳超时阈值：**10 秒**
- 超时后设备标记为离线，仪表盘不再显示
- 设备记录保留 **60 秒**后自动清除

---

## API 接口文档

### 1. 抓拍图片上传

边缘设备检测到跌倒事件时，上传抓拍图片。

- **URL**: `POST /capture/upload`
- **Content-Type**: `multipart/form-data`
- **前置条件**: MQTT 服务器已连接
- **参数**:

| 参数 | 类型 | 必填 | 说明 |
|------|------|------|------|
| `file` | File | 是 | 抓拍图片（支持 png/jpg/jpeg/gif） |
| `camera_id` | String | 是 | 摄像头编号 |
| `location` | String | 是 | 事件地点 |
| `violation_type` | String | 是 | 告警类型（如：跌倒告警） |

**调用示例 (Python)**：
```python
import requests

url = "http://10.60.83.159:5002/capture/upload"
files = {'file': open('fall_detected.jpg', 'rb')}
data = {
    'camera_id': '001',
    'location': '校园大门',
    'violation_type': '跌倒告警'
}
response = requests.post(url, files=files, data=data)
print(response.json())
# {"code": 200, "message": "上传成功"}
```

### 2. MQTT 连接管理

| 接口 | 方法 | 说明 |
|------|------|------|
| `/settings/api/mqtt/status` | GET | 获取 MQTT 连接状态 |
| `/settings/api/mqtt/connect` | POST | 连接 MQTT 服务器 |
| `/settings/api/mqtt/disconnect` | POST | 断开 MQTT 连接 |
| `/settings/api/mqtt/realtime-stats` | GET | 获取实时设备/摄像头统计数据 |
| `/settings/api/mqtt/configs` | GET | 获取保存的 MQTT 配置列表 |

### 3. ESP8266 云台控制

- **URL**: `POST /settings/api/esp8266/ptz`
- **Content-Type**: `application/json`
- **参数**:

| 参数 | 类型 | 说明 |
|------|------|------|
| `row` | Int | 行偏移（-10 ~ 10） |
| `col` | Int | 列偏移（-8 ~ 8） |

**控制方向**：

| 按键 | row 变化 | col 变化 |
|------|----------|----------|
| ← 左 | row - 1 | - |
| → 右 | row + 1 | - |
| ↑ 上 | - | col + 1 |
| ↓ 下 | - | col - 1 |
| 居中 | row = 0 | col = 0 |

### 4. 抓拍记录管理

| 接口 | 方法 | 说明 |
|------|------|------|
| `/capture/list` | GET | 获取抓拍记录列表（支持分页、筛选） |
| `/capture/delete/<id>` | POST | 删除单条记录（需管理员密码） |
| `/capture/batch-delete` | POST | 批量删除记录（需管理员密码） |
| `/capture/api/stats` | GET | 获取抓拍统计数据 |

### 5. 用户管理

| 接口 | 方法 | 说明 |
|------|------|------|
| `/auth/login` | GET/POST | 用户登录 |
| `/auth/register` | GET/POST | 用户注册（需邮箱验证码） |
| `/auth/logout` | POST | 用户注销 |

---

## 数据库模型

### User（用户表）

| 字段 | 类型 | 说明 |
|------|------|------|
| id | Integer | 主键 |
| username | String(80) | 用户名（唯一） |
| password | String(120) | Bcrypt 加密密码 |
| role | String(20) | 角色：admin / assistant / user |
| email | String(120) | 邮箱（唯一） |
| email_verified | Boolean | 邮箱是否验证 |
| avatar_url | String(255) | 头像 URL |
| signature | String(30) | 个人签名 |

### Capture（抓拍记录表）

| 字段 | 类型 | 说明 |
|------|------|------|
| id | Integer | 主键 |
| camera_id | String(50) | 摄像头 ID |
| location | String(100) | 拍摄地点 |
| image_path | String(255) | 图片存储路径 |
| thumbnail_path | String(255) | 缩略图路径 |
| violation_type | String(100) | 告警类型 |
| capture_time | DateTime | 抓拍时间 |

### MqttConfig（MQTT 配置表）

| 字段 | 类型 | 说明 |
|------|------|------|
| id | Integer | 主键 |
| broker | String(255) | MQTT 服务器地址 |
| port | Integer | 端口号（默认 1883） |
| username | String(100) | MQTT 用户名 |
| password | String(100) | MQTT 密码 |
| mediamtx_whip_port | Integer | MediaMTX WHIP 端口 |
| mediamtx_rtsp_port | Integer | MediaMTX RTSP 端口 |
| is_active | Boolean | 是否为当前活跃配置 |
| created_at | DateTime | 创建时间 |

---

## 角色权限说明

| 功能模块 | admin | assistant | user |
|----------|-------|-----------|------|
| 首页仪表盘 | ✅ | ✅ | ❌（独立首页） |
| 实时监控 | ✅ | ✅ | ❌ |
| 抓拍记录 | ✅ | ✅ | ✅ |
| 系统设置 | ✅ | ✅ | ❌ |
| 用户管理 | ✅ | ❌ | ❌ |
| PTZ 云台控制 | ✅ | ✅ | ❌ |
| 批量删除记录 | ✅ | ✅ | ✅ |

---

## 跌倒告警流程

```text
1. RK3588 边缘设备检测到跌倒
       │
       ├──→ MQTT: 发送消息到 rk3588lyh/001/report
       │    {"device_id":"rk3588lyh","camera_id":"001","detection":"falldown","count":5}
       │
       └──→ HTTP: 上传抓拍图片到 POST /capture/upload
            (file + camera_id + location + violation_type)

2. Web 服务处理
       │
       ├── MQTT 消息到达
       │    ├── 解析 JSON，检测 detection == "falldown"
       │    └── 异步发送邮件告警
       │         ├── 从数据库查询该摄像头最近的抓拍图片
       │         └── 发送邮件到 TARGET_EMAIL（带图片附件）
       │
       └── HTTP 上传到达
            ├── 保存原图到 static/captures/
            ├── 生成缩略图到 static/captures/thumbnails/
            ├── 写入 Capture 数据库记录
            └── Socket.IO 实时推送到前端
                 └── 前端弹窗通知 + 声音报警
```

---

## 配置文件说明 (`config.py`)

```python
# 安全密钥
SECRET_KEY = 'asdmmnkdnlamdl;awwd'

# 数据库配置
HOSTNAME = '127.0.0.1'
PORT = 3306
USERNAME = 'root'
PASSWORD = 'heweijie'
DATABASE = 'lyh'

# 邮箱配置（用于跌倒告警通知）
MAIL_SERVER = 'smtp.qq.com'
MAIL_USE_SSL = True
MAIL_PORT = 465
MAIL_USERNAME = '2930405291@qq.com'
MAIL_PASSWORD = 'soestqmovlwsdcfb'    # SMTP 授权码
MAIL_DEFAULT_SENDER = '2930405291@qq.com'

# 告警目标邮箱
TARGET_EMAIL = ['2931663895@qq.com']

# MQTT 默认配置
MQTT_BROKER = '10.60.83.159'
MQTT_PORT = 1883
MQTT_TOPIC_PREFIX = 'rk3588lyh/camera'

# ESP8266 设备 ID
ESP8266_ID = '002'
```

---

## 常见问题

**Q: 监控页面看不到视频画面？**
A: 确保设备已通过 MQTT 上放心跳包（rk3588lyh/info），且心跳中的 `http_url` 可从浏览器访问。非 localhost 环境可能需要 HTTPS。

**Q: 抓拍图片上传返回 503？**
A: 需要先在系统设置页面连接 MQTT 服务器，上传接口会检查 MQTT 连接状态。

**Q: 收不到跌倒告警邮件？**
A: 检查以下几点：
1. `config.py` 中 `MAIL_USERNAME` 和 `MAIL_PASSWORD` 是否正确
2. `TARGET_EMAIL` 是否配置了目标邮箱
3. SMTP 授权码是否有效（QQ 邮箱需要单独申请）
4. 远程设备是否同时发送了 MQTT 告警和 HTTP 图片上传

**Q: MQTT 连接状态显示断开，但实际能收到消息？**
A: 已修复。系统现在会通过 `client.is_connected()` 检查实际连接状态，而非仅依赖回调变量。

**Q: 数据库表没有创建？**
A: 表在应用启动时通过 `db.create_all()` 自动创建。确保 `config.py` 中的数据库连接参数正确，且已手动创建 `lyh` 数据库。

**Q: 如何创建管理员账号？**
A: 注册的账号默认角色为 `user`，需要通过 `add_admin.py` 脚本或在数据库中手动修改 `role` 字段为 `admin`。

---

## 开发指南

- 新增功能与代码规范：请阅读 [AGENTS.md](AGENTS.md)
- 本项目遵循 **Flask Blueprint 蓝图分离** 设计，所有新增模块在 `blueprints/` 下创建
- 模型变更需在 `models.py` 中实现，修改字段后需手动 `ALTER TABLE` 或清库重启
- 新增无需登录的路由必须在 `blueprints/__init__.py` 的 `allowed_routes` 白名单中添加

---

## 开源许可证

本项目基于 [MIT License](LICENSE) 开源。
