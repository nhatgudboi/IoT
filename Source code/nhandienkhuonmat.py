import cv2
import face_recognition
import firebase_admin
from firebase_admin import credentials, db
import numpy as np
import time

# --- CẤU HÌNH ---
DATABASE_URL = 'https://smarthome-iot-2d485-default-rtdb.asia-southeast1.firebasedatabase.app/'
KEY_PATH = "serviceAccountKey.json"
IMAGE_PATH = "images/admin.jpg"  # <-- ĐƯỜNG DẪN ĐẾN ẢNH CỦA BẠN

# --- THAM SỐ ĐỘ CHÍNH XÁC (QUAN TRỌNG) ---
FACE_TOLERANCE = 0.35  # Giảm từ 0.5 xuống 0.35 để nghiêm ngặt hơn (càng thấp càng chính xác)
FACE_DISTANCE_THRESHOLD = 0.35  # Ngưỡng khoảng cách tối đa (càng thấp càng chính xác)
REQUIRED_CONSECUTIVE_MATCHES = 8  # Phải nhận diện đúng 8 frame liên tiếp mới mở cửa

# --- 1. KẾT NỐI FIREBASE ---
try:
    cred = credentials.Certificate(KEY_PATH)
    firebase_admin.initialize_app(cred, {'databaseURL': DATABASE_URL})
    print("Ket noi Firebase OK!")
except Exception as e:
    print(f"Loi Firebase: {e}")
    exit()

# --- 2. HỌC KHUÔN MẶT CHỦ NHÂN (CẢI THIỆN ĐỘ CHÍNH XÁC) ---
print("Dang load anh chu nhan...")
try:
    # Hỗ trợ load nhiều ảnh để tăng độ chính xác
    import os
    import glob
    
    known_face_encodings = []
    known_face_names = []
    
    # Load ảnh chính
    img_bgr = cv2.imread(IMAGE_PATH)
    if img_bgr is None:
        print(f"LOI: Khong tim thay file anh tai: {IMAGE_PATH}")
        print("Hay kiem tra lai ten file va thu muc images.")
        exit()
    
    admin_image = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
    encodings = face_recognition.face_encodings(admin_image)
    
    if len(encodings) == 0:
        print("LOI: Khong tim thay khuon mat nao trong anh admin.jpg!")
        print("Hay chup anh khac ro net hon, nhin thang vao camera.")
        exit()
    
    # Thêm tất cả encodings từ ảnh chính (có thể có nhiều góc)
    for encoding in encodings:
        known_face_encodings.append(encoding)
        known_face_names.append("ADMIN")
    
    # Tự động load thêm các ảnh trong thư mục images/admin_*.jpg
    image_dir = os.path.dirname(IMAGE_PATH) if os.path.dirname(IMAGE_PATH) else "images"
    additional_images = glob.glob(os.path.join(image_dir, "admin_*.jpg"))
    additional_images.extend(glob.glob(os.path.join(image_dir, "admin_*.png")))
    
    if additional_images:
        print(f"Tim thay {len(additional_images)} anh bo sung. Dang load...")
        for img_path in additional_images:
            try:
                img_bgr = cv2.imread(img_path)
                if img_bgr is not None:
                    img_rgb = cv2.cvtColor(img_bgr, cv2.COLOR_BGR2RGB)
                    encodings = face_recognition.face_encodings(img_rgb)
                    for encoding in encodings:
                        known_face_encodings.append(encoding)
                        known_face_names.append("ADMIN")
                    print(f"  ✓ Da load: {os.path.basename(img_path)}")
            except Exception as e:
                print(f"  ✗ Loi khi load {img_path}: {e}")
    
    print(f"Da hoc xong {len(known_face_encodings)} khuon mat Admin!")
    print(f"Tham so: Tolerance={FACE_TOLERANCE}, Distance Threshold={FACE_DISTANCE_THRESHOLD}")
    print(f"Can {REQUIRED_CONSECUTIVE_MATCHES} frame lien tiep de mo cua.")

except Exception as e:
    print(f"Co loi xay ra khi xu ly anh: {e}")
    exit()

# --- 3. KHỞI ĐỘNG CAMERA ---
video_capture = cv2.VideoCapture(0)

door_opened = False
unlock_time = 0
process_this_frame = True # Biến để giảm tải (xử lý 1 khung hình, bỏ qua 1 khung hình)

# Lưu trữ face_locations và face_names để hiển thị khi không process frame
face_locations = []
face_names = []
face_confidences = []  # Lưu độ tin cậy

# Đếm số frame nhận diện đúng liên tiếp
consecutive_matches = 0
last_match_time = 0

print("---------------------------------------")
print("HE THONG FACE ID DA KICH HOAT")
print("Nhan 'q' de thoat.")
print("---------------------------------------")

while True:
    ret, frame = video_capture.read()
    if not ret: 
        print("Khong the doc camera!")
        break

    # Giảm kích thước ảnh xuống 1/4 để xử lý nhanh hơn
    small_frame = cv2.resize(frame, (0, 0), fx=0.25, fy=0.25)
    
    # Chuyển màu từ BGR (OpenCV) sang RGB (face_recognition)
    rgb_small_frame = cv2.cvtColor(small_frame, cv2.COLOR_BGR2RGB)
    
    # Chỉ xử lý mỗi frame thứ 2 để tiết kiệm CPU
    if process_this_frame:
        # Tìm tất cả khuôn mặt trong khung hình hiện tại
        face_locations = face_recognition.face_locations(rgb_small_frame)
        face_encodings = face_recognition.face_encodings(rgb_small_frame, face_locations)
        
        # Reset face_names và confidences mỗi lần process
        face_names = []
        face_confidences = []
        current_frame_has_match = False

        for face_encoding in face_encodings:
            # So sánh với khuôn mặt Admin với tolerance nghiêm ngặt hơn
            matches = face_recognition.compare_faces(known_face_encodings, face_encoding, tolerance=FACE_TOLERANCE)
            name = "Unknown"
            confidence = 0.0

            # Tính độ sai lệch (càng thấp càng giống)
            if len(known_face_encodings) > 0:
                face_distances = face_recognition.face_distance(known_face_encodings, face_encoding)
                best_match_index = np.argmin(face_distances)
                best_distance = face_distances[best_match_index]
                
                # Tính confidence (1 - distance, càng cao càng tốt)
                confidence = 1 - best_distance
                
                # CHỈ NHẬN DIỆN NẾU:
                # 1. Match = True (tolerance check)
                # 2. Distance < threshold (nghiêm ngặt hơn)
                if matches[best_match_index] and best_distance < FACE_DISTANCE_THRESHOLD:
                    name = known_face_names[best_match_index]
                    current_frame_has_match = True
                else:
                    # Nếu không đạt yêu cầu, reset counter
                    if consecutive_matches > 0:
                        print(f"⚠️  Khong du chinh xac! Distance: {best_distance:.3f}, Confidence: {confidence:.1%}")
                    consecutive_matches = 0

            face_names.append(name)
            face_confidences.append(confidence)
        
        # Xử lý logic đếm frame liên tiếp
        if current_frame_has_match:
            consecutive_matches += 1
            last_match_time = time.time()
            
            if consecutive_matches >= REQUIRED_CONSECUTIVE_MATCHES:
                # Đã nhận diện đúng đủ số frame liên tiếp
                current_time = time.time()
                if not door_opened or (current_time - unlock_time > 5):
                    print(f"✅ XAC NHAN! {consecutive_matches} frame lien tiep - CHAO MUNG ADMIN!")
                    print(f"   Confidence: {max(face_confidences):.1%}")
                    try:
                        ref = db.reference('/smarthome/commands')
                        ref.update({'ai_door': True})
                        door_opened = True
                        unlock_time = current_time
                        consecutive_matches = 0  # Reset sau khi mở cửa
                        print("✅ Da gui lenh mo cua thanh cong!")
                    except Exception as e:
                        print(f"❌ LOI khi gui lenh Firebase: {e}")
            else:
                # Đang đếm frame, hiển thị tiến trình
                if consecutive_matches % 2 == 0:  # Chỉ in mỗi 2 frame để không spam
                    print(f"⏳ Dang xac nhan... ({consecutive_matches}/{REQUIRED_CONSECUTIVE_MATCHES} frame)")
        else:
            # Không có match trong frame này, reset counter
            if consecutive_matches > 0:
                print(f"❌ Mat ket noi! Reset counter ({consecutive_matches} -> 0)")
            consecutive_matches = 0
    else:
        # Nếu không process frame này, giữ nguyên face_locations và face_names từ frame trước
        # Nhưng chỉ hiển thị, không xử lý logic mở cửa
        pass

    # Reset trạng thái mở cửa sau 5 giây (để sẵn sàng cho lần sau)
    if door_opened and (time.time() - unlock_time > 5):
        door_opened = False
        print("Reset trang thai mo cua. San sang nhan dien lan tiep theo.")

    process_this_frame = not process_this_frame

    # --- HIỂN THỊ KẾT QUẢ LÊN MÀN HÌNH ---
    # Đảm bảo face_locations và face_names cùng độ dài
    if len(face_locations) == len(face_names) == len(face_confidences):
        for idx, ((top, right, bottom, left), name, confidence) in enumerate(zip(face_locations, face_names, face_confidences)):
            # Vì lúc nãy thu nhỏ 1/4, giờ phải nhân 4 lên để vẽ đúng vị trí
            top *= 4
            right *= 4
            bottom *= 4
            left *= 4

            # Xác định màu và độ dày dựa trên độ chính xác
            if name == "ADMIN":
                color = (0, 255, 0)  # Xanh lá
                thickness = 3
                label_text = f"{name} ({confidence:.0%})"
            else:
                color = (0, 0, 255)  # Đỏ
                thickness = 2
                label_text = f"{name} ({confidence:.0%})"
            
            # Vẽ khung hình chữ nhật
            cv2.rectangle(frame, (left, top), (right, bottom), color, thickness)

            # Vẽ tên và confidence
            cv2.rectangle(frame, (left, bottom - 50), (right, bottom), color, cv2.FILLED)
            font = cv2.FONT_HERSHEY_DUPLEX
            cv2.putText(frame, label_text, (left + 6, bottom - 25), font, 0.6, (255, 255, 255), 1)
            
            # Hiển thị tiến trình nhận diện
            if name == "ADMIN":
                progress_text = f"{consecutive_matches}/{REQUIRED_CONSECUTIVE_MATCHES}"
                cv2.putText(frame, progress_text, (left + 6, bottom - 6), font, 0.5, (255, 255, 255), 1)
    
    # Hiển thị thông tin tổng quan ở góc trên
    info_text = f"Matches: {consecutive_matches}/{REQUIRED_CONSECUTIVE_MATCHES}"
    cv2.putText(frame, info_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (255, 255, 255), 2)
    cv2.putText(frame, info_text, (10, 30), cv2.FONT_HERSHEY_SIMPLEX, 0.7, (0, 0, 0), 1)

    cv2.imshow('Smart Home Face ID', frame)

    if cv2.waitKey(1) & 0xFF == ord('q'):
        break

video_capture.release()
cv2.destroyAllWindows()