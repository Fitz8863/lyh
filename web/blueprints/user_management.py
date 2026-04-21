from flask import Blueprint, render_template, request, jsonify, redirect, url_for, flash, current_app
import os
from werkzeug.utils import secure_filename
from flask_login import login_required, current_user
from flask_bcrypt import Bcrypt
from .models import User
from . import db
from .auth import super_admin_required

user_mgmt_bp = Blueprint('user_mgmt', __name__, url_prefix='/users')

# 上传头像的文件大小上限（2MB）
MAX_AVATAR_SIZE = 2 * 1024 * 1024

@user_mgmt_bp.route('/api/avatar/upload', methods=['POST'])
@login_required
def upload_avatar():
    """用户上传头像接口"""
    if 'avatar' not in request.files:
        return jsonify({'error': '未找到文件'}), 400
    
    file = request.files['avatar']
    if file.filename == '':
        return jsonify({'error': '未选择文件'}), 400
    
    # 尝试获取文件大小，防止超大文件上传
    # 额外兜底：如果请求体超过 2MB，直接拒绝，避免对未能正确读取大小的上传进行耗时处理
    MAX_SIZE = MAX_AVATAR_SIZE
    if getattr(request, 'content_length', None) is not None and request.content_length > MAX_SIZE:
        return jsonify({'error': '头像大小不能超过 2MB'}), 400
    
    try:
        file.seek(0, os.SEEK_END)
        size = file.tell()
        file.seek(0)
    except Exception:
        size = None
    if size is not None and size > MAX_AVATAR_SIZE:
        return jsonify({'error': '头像大小不能超过 2MB'}), 400
    
    if file:
        filename = secure_filename(f"avatar_{current_user.id}_{file.filename}")
        upload_path = os.path.join(current_app.static_folder, 'avatars', filename)
        
        # 确保目录存在
        os.makedirs(os.path.dirname(upload_path), exist_ok=True)
        
        file.save(upload_path)
        
        # 更新用户数据库
        current_user.avatar_url = f"avatars/{filename}"
        try:
            db.session.commit()
            return jsonify({
                'message': '头像上传成功',
                'url': url_for('static', filename=current_user.avatar_url)
            }), 200
        except Exception as e:
            db.session.rollback()
            return jsonify({'error': str(e)}), 500
    
    return jsonify({'error': '上传失败'}), 400


@user_mgmt_bp.before_request
@login_required
@super_admin_required
def before_request():
    if request.endpoint in [
        'user_mgmt.upload_avatar',
        'user_mgmt.get_profile',
        'user_mgmt.update_profile'
    ]:
        return


@user_mgmt_bp.route('/api/profile', methods=['GET'])
@login_required
def get_profile():
    """获取当前登录用户资料"""
    return jsonify({
        'username': current_user.username,
        'avatar_url': url_for('static', filename=current_user.avatar_url) if current_user.avatar_url else None,
        'signature': current_user.signature or ''
    }), 200


@user_mgmt_bp.route('/api/profile', methods=['POST'])
@login_required
def update_profile():
    """更新当前登录用户资料"""
    data = request.get_json(silent=True) or {}
    signature = (data.get('signature') or '').strip()

    if len(signature) > 30:
        return jsonify({'error': '个性签名不能超过30个字'}), 400

    current_user.signature = signature

    try:
        db.session.commit()
        return jsonify({'message': '个性签名已更新', 'signature': current_user.signature or ''}), 200
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@user_mgmt_bp.route('/')
def index():
    """用户管理主页"""
    users = User.query.all()
    return render_template('user_management.html', users=users)

@user_mgmt_bp.route('/api/list', methods=['GET'])
def list_users():
    """获取用户列表API"""
    users = User.query.all()
    return jsonify({
        'users': [{
            'id': u.id,
            'username': u.username,
            'role': u.role
        } for u in users]
    }), 200

@user_mgmt_bp.route('/api/add', methods=['POST'])
def add_user():
    """新增用户API"""
    data = request.json
    username = data.get('username')
    password = data.get('password')
    role = data.get('role', 'user')
    
    if not username or not password:
        return jsonify({'error': '用户名和密码不能为空'}), 400
        
    if User.query.filter_by(username=username).first():
        return jsonify({'error': '用户名已存在'}), 400

    if role == 'admin':
         return jsonify({'error': '不能添加额外的超级管理员'}), 403
        
    bcrypt = Bcrypt(current_app._get_current_object())
    hashed_password = bcrypt.generate_password_hash(password).decode('utf-8')
    
    new_user = User(username=username, password=hashed_password, role=role)
    try:
        db.session.add(new_user)
        db.session.commit()
        return jsonify({'message': '用户添加成功'}), 200
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@user_mgmt_bp.route('/api/update/<int:user_id>', methods=['POST'])
def update_user(user_id):
    """更新用户信息API"""
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404
        
    data = request.json
    username = data.get('username')
    password = data.get('password')
    role = data.get('role')
    
    # 防止修改超级管理员 heweijie 的角色
    if user.username == 'heweijie' and role and role != 'admin':
        return jsonify({'error': '不能修改超级管理员的角色'}), 403
    
    if user.username != 'heweijie' and role == 'admin':
        return jsonify({'error': '超级管理员有且仅有一个，不能设置为该角色'}), 403
        
    if username:
        existing_user = User.query.filter_by(username=username).first()
        if existing_user and existing_user.id != user_id:
            return jsonify({'error': '用户名已存在'}), 400
        user.username = username
        
    if password:
        bcrypt = Bcrypt(current_app._get_current_object())
        user.password = bcrypt.generate_password_hash(password).decode('utf-8')
        
    if role:
        user.role = role
        
    try:
        db.session.commit()
        return jsonify({'message': '用户信息更新成功'}), 200
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500

@user_mgmt_bp.route('/api/delete/<int:user_id>', methods=['DELETE'])
def delete_user(user_id):
    """删除用户API"""
    user = User.query.get(user_id)
    if not user:
        return jsonify({'error': '用户不存在'}), 404
        
    if user.username == 'heweijie':
        return jsonify({'error': '不能删除超级管理员'}), 403
        
    try:
        db.session.delete(user)
        db.session.commit()
        return jsonify({'message': '用户已删除'}), 200
    except Exception as e:
        db.session.rollback()
        return jsonify({'error': str(e)}), 500
