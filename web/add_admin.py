from app import app
from blueprints import db
from blueprints.models import User
from flask_bcrypt import Bcrypt

def add_super_admin():
    with app.app_context():
        username = 'lyh'
        password = '123456'
        role = 'admin'
        
        # 检查用户是否已存在
        existing_user = User.query.filter_by(username=username).first()
        if existing_user:
            print(f"用户 {username} 已存在。")
            return
            
        bcrypt = Bcrypt(app)
        hashed_password = bcrypt.generate_password_hash(password).decode('utf-8')
        
        new_user = User(username=username, password=hashed_password, role=role)
        try:
            db.session.add(new_user)
            db.session.commit()
            print(f"成功添加超级管理员: {username}")
        except Exception as e:
            db.session.rollback()
            print(f"添加用户失败: {str(e)}")

if __name__ == "__main__":
    add_super_admin()
