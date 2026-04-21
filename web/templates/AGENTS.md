# templates/ - Jinja2 模板层

## 概览
所有页面均继承 `base.html`，使用 Bootstrap 5 深色毛玻璃主题。

## 文件职责

| 文件 | 页面 | 访问权限 |
|------|------|----------|
| `base.html` | 全局布局（导航栏、Flash、报警、个人资料弹窗、SocketIO） | — |
| `index.html` | 管理员首页仪表盘 | admin/assistant |
| `user_index.html` | 普通用户首页（访客仪表盘） | user |
| `login.html` | 登录页 | 公开 |
| `register.html` | 注册页（含邮箱验证码） | 公开 / admin 创建用户 |
| `monitor.html` | 实时视频监控 + PTZ + 对讲 | 登录用户 |
| `alerts.html` | 抓拍告警记录流 | 登录用户 |
| `settings.html` | MQTT/设备/对讲配置 | 登录用户 |
| `user_management.html` | 用户 CRUD 管理 | 仅 super_admin |

## 模板继承规则

```
base.html
├── index.html
├── user_index.html
├── login.html
├── register.html  (注意: 独立加载 Bootstrap JS，未用 base.html 的)
├── monitor.html
├── alerts.html
├── settings.html
└── user_management.html
```

### 可用块
- `{% block title %}` — 页面标题
- `{% block content %}` — 主体内容（必须覆盖）
- `{% block extra_css %}` — 页面级 CSS（`<style>` 标签）
- `{% block extra_js %}` — 页面级 JS（`<script>` 标签）

## base.html 内置组件
修改前务必了解，这些已在基础模板中全局加载：

1. **导航栏** — 角色条件渲染：`current_user.is_super_admin` 控制"用户管理"入口
2. **Flash 消息** — 自动展示 `get_flashed_messages()`
3. **个人资料弹窗** (`#profileModal`) — 头像 + 签名编辑
4. **报警声音系统** (`AlarmSound` 类) — Web Audio API 生成警报音
5. **报警按钮** (`#alarm-btn`) — 左下角静音/取消静音切换
6. **停止报警按钮** (`#stop-alarm-btn`) — 居中大按钮，报警时显示
7. **通知容器** (`#notification-container`) — 右下角 SocketIO 实时弹窗
8. **SocketIO 连接** — 监听 `new_capture` 事件触发通知+报警

## 角色条件渲染
```jinja2
{# 导航栏中仅超管可见的入口 #}
{% if current_user.is_authenticated and current_user.is_super_admin %}
    <li>用户管理</li>
{% endif %}

{# 首页路由根据角色分流（在 main.py 中） #}
{# admin/assistant → index.html，user → user_index.html #}
```

## 样式规范
- **全局样式**: `static/css/style.css`（CSS 变量、卡片、按钮、表单等）
- **页面局部样式**: 写在 `{% block extra_css %}` 的 `<style>` 标签内
- **主题变量**: `--primary-color`, `--accent-color`, `--bg-card`, `--border-color` 等，定义在 style.css `:root`
- 所有卡片使用 `backdrop-filter: blur()` 毛玻璃效果
- 按钮统一圆角 `border-radius: 20px`

## 反模式（禁止）
- 新页面不继承 `base.html` → 丢失导航栏、报警系统、SocketIO
- 在 `extra_css` 中覆盖全局 CSS 变量 → 影响其他组件
- 重复引入 Bootstrap/SocketIO CDN → base.html 已全局加载
