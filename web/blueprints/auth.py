import functools
import time
import random
from flask import Blueprint, render_template, redirect, url_for, request, flash, current_app, abort, session, jsonify
from sqlalchemy import or_
from flask_login import login_user, login_required, logout_user, current_user
from flask_bcrypt import Bcrypt
from flask_mail import Message
from .models import User
from . import db
from exts import mail

auth_bp = Blueprint('auth', __name__, url_prefix='/')

def admin_required(f):
    @functools.wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_authenticated or current_user.role not in ['admin', 'assistant']:
            abort(403)
        return f(*args, **kwargs)
    return decorated_function

def super_admin_required(f):
    @functools.wraps(f)
    def decorated_function(*args, **kwargs):
        if not current_user.is_authenticated or current_user.role != 'admin':
            abort(403)
        return f(*args, **kwargs)
    return decorated_function

@auth_bp.route('/register', methods=['GET', 'POST'])
def register():
    is_admin_creation = current_user.is_authenticated and current_user.is_admin

    if current_user.is_authenticated:
        # 已认证用户，包括管理员，仍可通过管理员入口创建普通账户
        if is_admin_creation:
            pass  # 继续执行，进入普通注册/管理员创建分支
        else:
            return redirect(url_for('main.index'))

    if request.method == 'POST':
        # 管理员创建新用户（跳过邮箱验证码）
        if is_admin_creation:
            username = (request.form.get('username') or '').strip()
            password = request.form.get('password')
            confirm_password = request.form.get('confirm_password')
            email = (request.form.get('email') or '').strip() or None

            if not username or not password:
                flash('请填写完整的用户名和密码', 'danger')
                return render_template('register.html', admin_creation=is_admin_creation)

            if password != confirm_password:
                flash('两次输入的密码不一致', 'danger')
                return render_template('register.html', admin_creation=is_admin_creation)

            if User.query.filter_by(username=username).first():
                flash('用户名已存在', 'danger')
                return render_template('register.html', admin_creation=is_admin_creation)

            if email and User.query.filter_by(email=email).first():
                flash('邮箱已存在', 'danger')
                return render_template('register.html', admin_creation=is_admin_creation)

            bcrypt = Bcrypt(current_app._get_current_object())
            hashed_password = bcrypt.generate_password_hash(password).decode('utf-8')
            user = User(username=username, password=hashed_password, role='user', email=email, email_verified=False)

            try:
                db.session.add(user)
                db.session.commit()
                flash('新用户创建成功！', 'success')
                return redirect(url_for('main.index'))
            except Exception:
                db.session.rollback()
                flash('用户名/邮箱已存在', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        # 公众用户，执行邮箱验证码流程
        username = (request.form.get('username') or '').strip()
        password = request.form.get('password')
        confirm_password = request.form.get('confirm_password')
        email = (request.form.get('email') or '').strip().lower()
        code = (request.form.get('code') or '').strip()

        if not username or not password:
            flash('请填写完整的注册信息', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        if not email or not code:
            flash('请提供邮箱和验证码', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        if password != confirm_password:
            flash('两次输入的密码不一致', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        if User.query.filter_by(username=username).first():
            flash('用户名已存在', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        if User.query.filter_by(email=email).first():
            flash('邮箱已被注册', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        # 验证验证码是否有效
        ver = session.get('email_verif') or {}
        if not ver or ver.get('email') != email or ver.get('code') != code or ver.get('expires_at', 0) < time.time():
            flash('验证码无效或已过期，请重新获取验证码', 'danger')
            return render_template('register.html', admin_creation=is_admin_creation)

        bcrypt = Bcrypt(current_app._get_current_object())
        hashed_password = bcrypt.generate_password_hash(password).decode('utf-8')
        user = User(username=username, password=hashed_password, role='user', email=email, email_verified=True)

        try:
            db.session.add(user)
            db.session.commit()
            # 验证成功后清除验证码
            session.pop('email_verif', None)
            flash('注册成功!请登录', 'success')
            return redirect(url_for('auth.login'))
        except Exception:
            db.session.rollback()
            flash('注册失败，请检查用户名或邮箱是否重复', 'danger')
        return render_template('register.html', admin_creation=is_admin_creation)

    return render_template('register.html', admin_creation=is_admin_creation)

@auth_bp.route('/login', methods=['GET', 'POST'])
def login():
    if current_user.is_authenticated:
        return redirect(url_for('main.index'))
    if request.method == 'POST':
        username = request.form.get('username')
        password = request.form.get('password')
        remember = request.form.get('remember') == 'on'
        
        bcrypt = Bcrypt(current_app._get_current_object())
        # 支持使用用户名或邮箱进行登录
        user = User.query.filter(or_(User.username == username, User.email == username)).first()
        if user and bcrypt.check_password_hash(user.password, password):
            login_user(user, remember=remember)
            return redirect(url_for('main.index'))
        else:
            flash('用户名或密码错误', 'danger')
    return render_template('login.html')

@auth_bp.route('/logout')
@login_required
def logout():
    logout_user()
    return redirect(url_for('main.index'))


@auth_bp.route('/register/send_code', methods=['POST'])
def send_verification_code():
    # 仅允许未登录用户请求验证码
    if current_user.is_authenticated:
        flash('请先退出当前账户再获取验证码', 'danger')
        return redirect(url_for('auth.register'))
    email = (request.form.get('email') or '').strip().lower()
    if not email:
        flash('请输入有效的邮箱地址', 'danger')
        return redirect(url_for('auth.register'))

    if User.query.filter_by(email=email).first():
        flash('该邮箱已被注册', 'danger')
        return redirect(url_for('auth.register'))

    # 60 秒 cooldown
    last_sent = session.get('email_verif_last_sent') or 0
    if time.time() - last_sent < 60:
        flash('验证码发送频率过高，请稍后再试', 'danger')
        return redirect(url_for('auth.register'))
    code = ''.join(random.choices('0123456789', k=6))
    expires_at = time.time() + 5 * 60
    session['email_verif'] = {'email': email, 'code': code, 'expires_at': expires_at}
    session['email_verif_last_sent'] = time.time()
    # 发送邮件
    try:
        msg = Message("VisionGuard 注册验证码", recipients=[email], body=f"您的注册验证码是 {code}，5分钟内有效。若非本人操作，请忽略。")
        mail.send(msg)
        flash('验证码已发送，请检查邮箱', 'success')
    except Exception as e:
        flash('验证码发送失败，请稍后重试', 'danger')
    return redirect(url_for('auth.register'))


@auth_bp.route('/api/send_signup_code', methods=['POST'])
def api_send_signup_code():
    # API 端点，供前端页面通过 JS 调用发送验证码
    data = request.get_json(silent=True) or {}
    email = (data.get('email') or '').strip().lower()
    if not email:
        return jsonify({'success': False, 'message': '请输入邮箱'}), 400

    if User.query.filter_by(email=email).first():
        return jsonify({'success': False, 'message': '该邮箱已被注册'}), 400

    last_sent = session.get('email_verif_last_sent') or 0
    if time.time() - last_sent < 60:
        return jsonify({'success': False, 'message': '请稍后再试'}), 429

    code = ''.join(random.choices('0123456789', k=6))
    expires_at = time.time() + 5 * 60
    session['email_verif'] = {'email': email, 'code': code, 'expires_at': expires_at}
    session['email_verif_last_sent'] = time.time()
    try:
        msg = Message("VisionGuard 注册验证码", recipients=[email], body=f"您的注册验证码是 {code}，5分钟内有效。若非本人操作，请忽略。")
        mail.send(msg)
        return jsonify({'success': True, 'message': '验证码已发送'})
    except Exception:
        return jsonify({'success': False, 'message': '发送失败，请稍后重试'}), 500
