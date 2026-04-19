// Trạng thái thiết bị
const deviceState = {
    gasValue: 0,
    lightSensor: 0, // 0 = Sáng, 1 = Tối
    alertLedOn: false,
    smartLightOn: false,
    smartLightAutoMode: true,
    aiDoorOpen: false,
    doorOpen: false,
    connected: false,
    lastAIOpen: null
};


// Face Recognition State
const faceRecognitionState = {
    isModelLoaded: false,
    isCameraActive: false,
    video: null,
    canvas: null,
    knownFaces: [], // Array of face descriptors
    lastDetection: null,
    detectionInterval: null,
    consecutiveMatches: 0, // Đếm số frame nhận diện đúng liên tiếp
    lastMatchTime: 0
};


// Tham số độ chính xác (đồng bộ với Python)
const FACE_TOLERANCE = 0.35; // Giảm từ 0.5 xuống 0.35 để nghiêm ngặt hơn
const FACE_DISTANCE_THRESHOLD = 0.35; // Ngưỡng khoảng cách tối đa
const REQUIRED_CONSECUTIVE_MATCHES = 8; // Phải nhận diện đúng 8 frame liên tiếp mới mở cửa


// Firebase database reference
const dbRef = database.ref('smarthome');


// Khởi tạo ứng dụng
document.addEventListener('DOMContentLoaded', async () => {
    initializeUI();
    startFirebaseListener();
    setupEventListeners();
    await initializeFaceRecognition();
    addLog('Hệ thống đã khởi động', 'success');
});


// Khởi tạo giao diện
function initializeUI() {
    updateGasDisplay(0);
    updateLightSensorDisplay(0);
    updateAlertLedDisplay(false);
    updateSmartLightDisplay(false, true);
    updateAIDisplay(false, null);
    updateDoorDisplay(false);
    updateAIWebDisplay('Chưa khởi động', 0);
    updateConnectionStatus(false);
}


// Thiết lập event listeners
function setupEventListeners() {
    // Smart Light Auto/Manual Toggle
    const autoModeToggle = document.getElementById('auto-mode-toggle');
    autoModeToggle.addEventListener('change', (e) => {
        const isAuto = e.target.checked;
        controlSmartLightMode(isAuto);
    });


    // Smart Light Manual Controls
    document.getElementById('smart-light-on').addEventListener('click', () => {
        controlSmartLight(true);
    });


    document.getElementById('smart-light-off').addEventListener('click', () => {
        controlSmartLight(false);
    });


    // AI Test Door Button
    document.getElementById('test-ai-door').addEventListener('click', () => {
        testAIDoor();
    });


    // Face Recognition Web Controls
    document.getElementById('start-camera').addEventListener('click', startCamera);
    document.getElementById('stop-camera').addEventListener('click', stopCamera);
    document.getElementById('train-face').addEventListener('click', () => {
        document.getElementById('face-image-input').click();
    });
    document.getElementById('face-image-input').addEventListener('change', handleTrainFace);
   
    // Clear logs button
    document.getElementById('clear-logs').addEventListener('click', () => {
        const logContainer = document.getElementById('log-container');
        logContainer.innerHTML = '<div class="log-item success">Nhật ký đã được xóa</div>';
        addLog('Nhật ký đã được xóa', '');
    });
}


// Bắt đầu lắng nghe dữ liệu từ Firebase (Real-time)
let lastDataTimestamp = 0;
let connectionCheckInterval = null;
const DATA_TIMEOUT = 3000; // 3 giây không có dữ liệu mới = mất kết nối (giảm từ 5s)


function startFirebaseListener() {
    // Kiểm tra kết nối Firebase trước
    checkFirebaseConnection();
   
    // Kiểm tra timeout định kỳ (mỗi 500ms để phản hồi cực nhanh)
    if (connectionCheckInterval) {
        clearInterval(connectionCheckInterval);
    }
   
    connectionCheckInterval = setInterval(() => {
        const now = Date.now();
        if (lastDataTimestamp > 0 && (now - lastDataTimestamp) > DATA_TIMEOUT) {
            updateConnectionStatus(false);
            deviceState.connected = false;
            lastDataTimestamp = 0; // Reset để tránh spam log
        }
    }, 500); // Kiểm tra mỗi 500ms để phản hồi nhanh
   
    // Kiểm tra dữ liệu ngay khi load trang (không đợi)
    dbRef.child('data').once('value', (snapshot) => {
        const data = snapshot.val();
        if (data && (data.gas !== undefined || data.light_sensor !== undefined)) {
            lastDataTimestamp = Date.now();
            updateConnectionStatus(true);
            deviceState.connected = true;
           
            // Cập nhật dữ liệu ngay
            deviceState.gasValue = data.gas || 0;
            deviceState.lightSensor = data.light_sensor || 0;
            updateGasDisplay(deviceState.gasValue);
            updateLightSensorDisplay(deviceState.lightSensor);
        } else {
            updateConnectionStatus(false);
            deviceState.connected = false;
        }
    }).catch((error) => {
        console.error('Lỗi kiểm tra dữ liệu ban đầu:', error);
        updateConnectionStatus(false);
        deviceState.connected = false;
    });
   
    // Lắng nghe real-time để cập nhật ngay khi có thay đổi
    dbRef.child('data').on('value', (snapshot) => {
        const data = snapshot.val();
       
        if (data && (data.gas !== undefined || data.light_sensor !== undefined)) {
            // Cập nhật timestamp ngay khi nhận được dữ liệu hợp lệ
            lastDataTimestamp = Date.now();
           
            deviceState.gasValue = data.gas || 0;
            deviceState.lightSensor = data.light_sensor || 0;
            deviceState.connected = true;


            // Cập nhật giao diện ngay lập tức
            updateGasDisplay(deviceState.gasValue);
            updateLightSensorDisplay(deviceState.lightSensor);
            updateConnectionStatus(true);
        } else {
            // Không có dữ liệu hợp lệ
            // Chỉ set offline nếu đã từng có dữ liệu trước đó
            if (lastDataTimestamp > 0) {
                updateConnectionStatus(false);
                deviceState.connected = false;
            }
        }
    }, (error) => {
        console.error('Lỗi Firebase:', error);
        updateConnectionStatus(false);
        deviceState.connected = false;
        addLog('Lỗi kết nối Firebase: ' + error.message, 'error');
    });


    // Lắng nghe trạng thái đèn thông minh
    dbRef.child('status/smart_light').on('value', (snapshot) => {
        if (snapshot.exists()) {
            deviceState.smartLightOn = snapshot.val();
            updateSmartLightDisplay(deviceState.smartLightOn, deviceState.smartLightAutoMode);
        }
    });


    // Lắng nghe lệnh AI door (để hiển thị trạng thái)
    dbRef.child('commands/ai_door').on('value', (snapshot) => {
        if (snapshot.exists()) {
            const aiCommand = snapshot.val();
            if (aiCommand === true) {
                deviceState.aiDoorOpen = true;
                deviceState.lastAIOpen = new Date();
                updateAIDisplay(true, deviceState.lastAIOpen);
                addLog('🤖 AI đã nhận diện và mở cửa!', 'success');
               
                // Reset sau 5 giây
                setTimeout(() => {
                    deviceState.aiDoorOpen = false;
                    updateAIDisplay(false, deviceState.lastAIOpen);
                }, 5000);
            } else {
                deviceState.aiDoorOpen = false;
                updateAIDisplay(false, deviceState.lastAIOpen);
            }
        }
    });


    // Tính toán trạng thái các thiết bị dựa trên gas value
    dbRef.child('data/gas').on('value', (snapshot) => {
        if (snapshot.exists()) {
            const gasValue = snapshot.val();
            const isDanger = gasValue >= 1000; // Đồng bộ với GAS_HIGH = 1000 trong .ino
           
            // Cập nhật trạng thái báo động
            deviceState.alertLedOn = isDanger;
            deviceState.doorOpen = isDanger; // Cửa mở khi có gas
           
            updateAlertLedDisplay(deviceState.alertLedOn);
            updateDoorDisplay(deviceState.doorOpen);
        }
    });
}


// Kiểm tra kết nối Firebase
function checkFirebaseConnection() {
    try {
        const connectedRef = database.ref('.info/connected');
        connectedRef.on('value', (snapshot) => {
            const isConnected = snapshot.val();
            if (!isConnected) {
                updateConnectionStatus(false);
                deviceState.connected = false;
                addLog('⚠️ Mất kết nối với Firebase', 'error');
            }
            // Nếu Firebase connected nhưng chưa có dữ liệu từ ESP32,
            // trạng thái sẽ được cập nhật bởi startFirebaseListener()
        });
    } catch (error) {
        console.error('Lỗi kiểm tra Firebase connection:', error);
        updateConnectionStatus(false);
        deviceState.connected = false;
    }
}


// Cập nhật hiển thị cảm biến gas
function updateGasDisplay(value) {
    const gasValueEl = document.getElementById('gas-value');
    const gasProgressEl = document.getElementById('gas-progress');
    const gasStatusEl = document.getElementById('gas-status');


    gasValueEl.textContent = value;


    // Tính phần trăm (max là 2000 PPM)
    const percentage = Math.min((value / 2000) * 100, 100);
    gasProgressEl.style.width = percentage + '%';


    // Cập nhật trạng thái và màu sắc
    if (value < 1000) {
        gasStatusEl.textContent = 'Bình thường';
        gasStatusEl.className = 'card-status';
        gasProgressEl.className = 'progress-fill';
    } else {
        gasStatusEl.textContent = 'Nguy hiểm';
        gasStatusEl.className = 'card-status danger';
        gasProgressEl.className = 'progress-fill danger';
        addLog(`⚠️ CẢNH BÁO! Nồng độ gas: ${value} PPM - Hệ thống đã tự động bật báo động`, 'error');
    }
}


// Cập nhật hiển thị cảm biến ánh sáng
function updateLightSensorDisplay(value) {
    const lightSensorStatusEl = document.getElementById('light-sensor-status');
    const lightSensorTextEl = document.getElementById('light-sensor-text');
    const sunIcon = document.getElementById('sun-icon');
    const moonIcon = document.getElementById('moon-icon');


    // value = 0 là Sáng, value = 1 (HIGH) là Tối
    if (value === 0 || value === false) {
        lightSensorStatusEl.textContent = 'Sáng';
        lightSensorStatusEl.className = 'card-status active';
        lightSensorTextEl.textContent = 'Môi trường sáng';
        sunIcon.classList.add('active');
        moonIcon.classList.remove('active');
    } else {
        lightSensorStatusEl.textContent = 'Tối';
        lightSensorStatusEl.className = 'card-status';
        lightSensorTextEl.textContent = 'Môi trường tối';
        sunIcon.classList.remove('active');
        moonIcon.classList.add('active');
    }
}


// Cập nhật hiển thị đèn báo động
function updateAlertLedDisplay(isOn) {
    const alertLedStatusEl = document.getElementById('alert-led-status');
    const alertLedIconEl = document.getElementById('alert-led-icon');


    if (isOn) {
        alertLedStatusEl.textContent = 'Đang bật';
        alertLedStatusEl.className = 'card-status danger';
        alertLedIconEl.classList.add('active');
    } else {
        alertLedStatusEl.textContent = 'Tắt';
        alertLedStatusEl.className = 'card-status';
        alertLedIconEl.classList.remove('active');
    }
}


// Cập nhật hiển thị đèn thông minh
function updateSmartLightDisplay(isOn, isAuto) {
    const smartLightStatusEl = document.getElementById('smart-light-status');
    const smartLightIconEl = document.getElementById('smart-light-icon');
    const autoModeToggle = document.getElementById('auto-mode-toggle');
    const modeText = document.getElementById('mode-text');
    const manualControl = document.getElementById('manual-control');


    deviceState.smartLightAutoMode = isAuto;
    autoModeToggle.checked = isAuto;
    modeText.textContent = isAuto ? 'Tự động' : 'Thủ công';


    // Hiển thị/ẩn nút điều khiển thủ công
    if (isAuto) {
        manualControl.style.display = 'none';
    } else {
        manualControl.style.display = 'flex';
    }


    if (isOn) {
        smartLightStatusEl.textContent = isAuto ? 'Bật (Tự động)' : 'Bật (Thủ công)';
        smartLightStatusEl.className = 'card-status active';
        smartLightIconEl.classList.add('active');
    } else {
        smartLightStatusEl.textContent = isAuto ? 'Tắt (Tự động)' : 'Tắt (Thủ công)';
        smartLightStatusEl.className = 'card-status';
        smartLightIconEl.classList.remove('active');
    }
}


// Cập nhật hiển thị AI Face Unlock
function updateAIDisplay(isActive, lastOpenTime) {
    const aiStatusEl = document.getElementById('ai-status');
    const aiStatusTextEl = document.getElementById('ai-status-text');
    const aiLastOpenEl = document.getElementById('ai-last-open');
    const aiIconEl = document.getElementById('ai-icon');


    if (isActive) {
        aiStatusEl.textContent = 'Đang mở cửa';
        aiStatusEl.className = 'card-status active';
        aiStatusTextEl.textContent = 'Đã nhận diện khuôn mặt';
        aiIconEl.classList.add('active');
       
        if (lastOpenTime) {
            const timeStr = lastOpenTime.toLocaleTimeString('vi-VN');
            aiLastOpenEl.textContent = timeStr;
        }
    } else {
        aiStatusEl.textContent = 'Sẵn sàng';
        aiStatusEl.className = 'card-status';
        aiStatusTextEl.textContent = 'Đang chờ nhận diện';
        aiIconEl.classList.remove('active');
       
        if (lastOpenTime) {
            const timeStr = lastOpenTime.toLocaleTimeString('vi-VN');
            aiLastOpenEl.textContent = timeStr;
        } else {
            aiLastOpenEl.textContent = 'Chưa có';
        }
    }
}


// Cập nhật hiển thị cửa
function updateDoorDisplay(isOpen) {
    const doorStatusEl = document.getElementById('door-status');
    const doorIconEl = document.getElementById('door-icon');


    if (isOpen) {
        doorStatusEl.textContent = 'Mở';
        doorStatusEl.className = 'card-status active';
        doorIconEl.classList.add('active');
    } else {
        doorStatusEl.textContent = 'Đóng';
        doorStatusEl.className = 'card-status';
        doorIconEl.classList.remove('active');
    }
}


// ========== FACE RECOGNITION WEB FUNCTIONS ==========


// Khởi tạo Face Recognition
async function initializeFaceRecognition() {
    try {
        addLog('Đang tải Face Recognition models...', '');
       
        // Load face-api models từ CDN
        const MODEL_URL = 'https://cdn.jsdelivr.net/npm/@vladmandic/face-api/model/';
        await faceapi.nets.tinyFaceDetector.loadFromUri(MODEL_URL);
        await faceapi.nets.faceLandmark68Net.loadFromUri(MODEL_URL);
        await faceapi.nets.faceRecognitionNet.loadFromUri(MODEL_URL);
       
        faceRecognitionState.isModelLoaded = true;
        faceRecognitionState.video = document.getElementById('video');
        faceRecognitionState.canvas = document.getElementById('canvas');
       
        updateAIWebDisplay('Sẵn sàng', faceRecognitionState.knownFaces.length);
        addLog('✅ Face Recognition models đã tải xong', 'success');
    } catch (error) {
        console.error('Lỗi load Face Recognition models:', error);
        addLog('❌ Lỗi tải Face Recognition models', 'error');
    }
}


// Bật camera
async function startCamera() {
    try {
        const stream = await navigator.mediaDevices.getUserMedia({
            video: {
                width: 640,
                height: 480,
                facingMode: 'user' // Front camera
            }
        });
       
        faceRecognitionState.video.srcObject = stream;
        faceRecognitionState.isCameraActive = true;
       
        document.getElementById('start-camera').style.display = 'none';
        document.getElementById('stop-camera').style.display = 'inline-block';
       
        updateAIWebDisplay('Đang quét...', faceRecognitionState.knownFaces.length);
        addLog('📷 Camera đã bật', 'success');
       
        // Bắt đầu detect faces
        startFaceDetection();
    } catch (error) {
        console.error('Lỗi bật camera:', error);
        addLog('❌ Không thể bật camera. Vui lòng cho phép truy cập camera.', 'error');
    }
}


// Tắt camera
function stopCamera() {
    if (faceRecognitionState.video.srcObject) {
        const tracks = faceRecognitionState.video.srcObject.getTracks();
        tracks.forEach(track => track.stop());
        faceRecognitionState.video.srcObject = null;
    }
   
    faceRecognitionState.isCameraActive = false;
   
    if (faceRecognitionState.detectionInterval) {
        clearInterval(faceRecognitionState.detectionInterval);
        faceRecognitionState.detectionInterval = null;
    }
   
    // Reset counter khi tắt camera
    faceRecognitionState.consecutiveMatches = 0;
   
    // Clear canvas
    const ctx = faceRecognitionState.canvas.getContext('2d');
    ctx.clearRect(0, 0, faceRecognitionState.canvas.width, faceRecognitionState.canvas.height);
   
    document.getElementById('start-camera').style.display = 'inline-block';
    document.getElementById('stop-camera').style.display = 'none';
   
    updateAIWebDisplay('Đã tắt', faceRecognitionState.knownFaces.length);
    addLog('📷 Camera đã tắt', '');
}


// Bắt đầu detect faces
function startFaceDetection() {
    if (!faceRecognitionState.isModelLoaded || !faceRecognitionState.isCameraActive) return;
   
    faceRecognitionState.detectionInterval = setInterval(async () => {
        await detectFaces();
    }, 500); // Detect mỗi 500ms
}


// Detect và nhận diện khuôn mặt
async function detectFaces() {
    if (!faceRecognitionState.video || !faceRecognitionState.isModelLoaded) return;
   
    const video = faceRecognitionState.video;
    const canvas = faceRecognitionState.canvas;
   
    // Set canvas size
    canvas.width = video.videoWidth;
    canvas.height = video.videoHeight;
   
    const ctx = canvas.getContext('2d');
    ctx.clearRect(0, 0, canvas.width, canvas.height);
   
    // Detect faces
    const detections = await faceapi
        .detectAllFaces(video, new faceapi.TinyFaceDetectorOptions())
        .withFaceLandmarks()
        .withFaceDescriptors();
   
    if (detections.length === 0) {
        return;
    }
   
    // Draw boxes và check recognition
    let currentFrameHasMatch = false;
   
    // Xử lý từng detection (không dùng async trong forEach)
    for (const detection of detections) {
        const box = detection.detection.box;
       
        // Check if face is recognized
        if (faceRecognitionState.knownFaces.length > 0) {
            const faceMatcher = new faceapi.FaceMatcher(faceRecognitionState.knownFaces, FACE_TOLERANCE);
            const bestMatch = faceMatcher.findBestMatch(detection.descriptor);
           
            // Tính confidence (1 - distance)
            const confidence = 1 - bestMatch.distance;
           
            // CHỈ NHẬN DIỆN NẾU:
            // 1. Match label !== 'unknown' (tolerance check)
            // 2. Distance < threshold (nghiêm ngặt hơn)
            const isRecognized = bestMatch.label !== 'unknown' && bestMatch.distance < FACE_DISTANCE_THRESHOLD;
           
            // Draw box với màu dựa trên độ chính xác
            ctx.strokeStyle = isRecognized ? '#10b981' : '#ef4444';
            ctx.lineWidth = isRecognized ? 3 : 2;
            ctx.strokeRect(box.x, box.y, box.width, box.height);
           
            // Draw label với confidence
            ctx.fillStyle = isRecognized ? '#10b981' : '#ef4444';
            ctx.fillRect(box.x, box.y - 50, box.width, 50);
            ctx.fillStyle = 'white';
            ctx.font = '14px Arial';
           
            if (isRecognized) {
                ctx.fillText(
                    `✅ ${bestMatch.label} (${Math.round(confidence * 100)}%)`,
                    box.x + 5,
                    box.y - 30
                );
                // Hiển thị tiến trình (sẽ cập nhật sau khi đếm)
                const nextMatchCount = faceRecognitionState.consecutiveMatches + 1;
                ctx.fillText(
                    `${nextMatchCount}/${REQUIRED_CONSECUTIVE_MATCHES}`,
                    box.x + 5,
                    box.y - 10
                );
                currentFrameHasMatch = true;
            } else {
                ctx.fillText(
                    `❌ Unknown (${Math.round(confidence * 100)}%)`,
                    box.x + 5,
                    box.y - 25
                );
            }
        } else {
            // Chưa có face nào được train
            ctx.strokeStyle = '#f59e0b';
            ctx.lineWidth = 3;
            ctx.strokeRect(box.x, box.y, box.width, box.height);
            ctx.fillStyle = '#f59e0b';
            ctx.fillRect(box.x, box.y - 25, box.width, 25);
            ctx.fillStyle = 'white';
            ctx.font = '16px Arial';
            ctx.fillText('⚠️ Chưa train', box.x + 5, box.y - 5);
        }
    }
   
    // Xử lý logic đếm frame liên tiếp (đồng bộ với Python)
    if (currentFrameHasMatch) {
        faceRecognitionState.consecutiveMatches++;
        faceRecognitionState.lastMatchTime = Date.now();
       
        if (faceRecognitionState.consecutiveMatches >= REQUIRED_CONSECUTIVE_MATCHES) {
            // Đã nhận diện đúng đủ số frame liên tiếp
            const now = Date.now();
            if (!faceRecognitionState.lastDetection || (now - faceRecognitionState.lastDetection) > 5000) {
                faceRecognitionState.lastDetection = now;
                faceRecognitionState.consecutiveMatches = 0; // Reset trước khi mở cửa
                await unlockDoor();
            }
        } else {
            // Đang đếm frame, cập nhật UI
            updateAIWebDisplay(`Đang xác nhận... (${faceRecognitionState.consecutiveMatches}/${REQUIRED_CONSECUTIVE_MATCHES})`, faceRecognitionState.knownFaces.length);
        }
    } else {
        // Không có match trong frame này, reset counter (giống Python)
        if (faceRecognitionState.consecutiveMatches > 0) {
            faceRecognitionState.consecutiveMatches = 0;
            updateAIWebDisplay('Đang quét...', faceRecognitionState.knownFaces.length);
        }
    }
}


// Train khuôn mặt từ ảnh (hỗ trợ train nhiều ảnh như Python)
async function handleTrainFace(event) {
    const files = event.target.files;
    if (!files || files.length === 0) return;
   
    try {
        const name = prompt('Nhập tên cho khuôn mặt này:', 'Admin');
        if (!name) {
            event.target.value = '';
            return;
        }
       
        addLog(`Đang train ${files.length} ảnh cho ${name}...`, '');
       
        let successCount = 0;
        const descriptors = [];
       
        // Xử lý từng file (hỗ trợ chọn nhiều ảnh)
        for (let i = 0; i < files.length; i++) {
            const file = files[i];
            try {
                const image = await faceapi.bufferToImage(file);
                const detections = await faceapi
                    .detectAllFaces(image, new faceapi.TinyFaceDetectorOptions())
                    .withFaceLandmarks()
                    .withFaceDescriptors();
               
                if (detections.length === 0) {
                    addLog(`⚠️ Không tìm thấy khuôn mặt trong ${file.name}`, 'warning');
                    continue;
                }
               
                // Thêm tất cả khuôn mặt tìm được (giống Python)
                detections.forEach(detection => {
                    descriptors.push(detection.descriptor);
                    successCount++;
                });
               
                addLog(`✓ Đã load: ${file.name} (${detections.length} khuôn mặt)`, 'success');
            } catch (error) {
                console.error(`Lỗi khi xử lý ${file.name}:`, error);
                addLog(`✗ Lỗi khi load ${file.name}`, 'error');
            }
        }
       
        if (descriptors.length > 0) {
            // Tạo LabeledFaceDescriptors với tất cả descriptors (giống Python)
            const labeledFaceDescriptor = new faceapi.LabeledFaceDescriptors(name, descriptors);
            faceRecognitionState.knownFaces.push(labeledFaceDescriptor);
           
            updateAIWebDisplay('Đã train', faceRecognitionState.knownFaces.length);
            addLog(`✅ Đã train ${successCount} khuôn mặt cho ${name}`, 'success');
        } else {
            addLog('❌ Không có khuôn mặt nào được train', 'error');
        }
       
        // Reset input
        event.target.value = '';
    } catch (error) {
        console.error('Lỗi train face:', error);
        addLog('❌ Lỗi train khuôn mặt', 'error');
    }
}


// Mở cửa khi nhận diện được
async function unlockDoor() {
    try {
        // CHỈ GỬI LỆNH MỞ (TRUE)
        // Việc đóng cửa và reset về false sẽ do ESP32 tự xử lý (để tránh xung đột)
        await database.ref('smarthome/commands/ai_door').set(true);
       
        addLog(`✅ XÁC NHẬN! ${REQUIRED_CONSECUTIVE_MATCHES} frame liên tiếp - CHÀO MỪNG ADMIN!`, 'success');
        updateAIWebDisplay('Đã nhận diện!', faceRecognitionState.knownFaces.length);
       
        // Không cần setTimeout set(false) ở đây nữa vì ESP32 đã có code tự reset sau 5s
    } catch (error) {
        console.error('Lỗi unlock door:', error);
        addLog('❌ Lỗi khi mở cửa', 'error');
    }
}


// Cập nhật hiển thị AI Web (đồng bộ với Python)
function updateAIWebDisplay(status, trainedCount) {
    const statusEl = document.getElementById('ai-web-status');
    const statusTextEl = document.getElementById('face-recognition-status');
    const countEl = document.getElementById('trained-faces-count');
    const progressEl = document.getElementById('matches-progress');
   
    statusEl.textContent = status;
    statusTextEl.textContent = status;
   
    // Đếm tổng số descriptors (giống Python)
    let totalDescriptors = 0;
    faceRecognitionState.knownFaces.forEach(face => {
        totalDescriptors += face.descriptors.length;
    });
    countEl.textContent = totalDescriptors || trainedCount;
   
    // Cập nhật tiến trình nhận diện
    if (progressEl) {
        progressEl.textContent = `${faceRecognitionState.consecutiveMatches}/${REQUIRED_CONSECUTIVE_MATCHES}`;
    }
   
    if (status.includes('Đã nhận diện')) {
        statusEl.className = 'card-status active';
    } else if (status.includes('Đang quét') || status.includes('Đang xác nhận')) {
        statusEl.className = 'card-status warning';
    } else {
        statusEl.className = 'card-status';
    }
}


// Điều khiển chế độ đèn thông minh (Auto/Manual)
async function controlSmartLightMode(isAuto) {
    try {
        await database.ref('smarthome/config/auto_mode').set(isAuto);
        deviceState.smartLightAutoMode = isAuto;
        updateSmartLightDisplay(deviceState.smartLightOn, isAuto);
        addLog(`Đèn thông minh chuyển sang chế độ: ${isAuto ? 'Tự động' : 'Thủ công'}`, 'success');
    } catch (error) {
        console.error('Lỗi Firebase:', error);
        addLog('Lỗi khi thay đổi chế độ đèn', 'error');
    }
}


// Điều khiển đèn thông minh (chỉ khi Manual mode)
async function controlSmartLight(on) {
    if (deviceState.smartLightAutoMode) {
        addLog('Vui lòng tắt chế độ tự động để điều khiển thủ công', 'warning');
        return;
    }


    try {
        await database.ref('smarthome/commands/smart_light').set(on);
        deviceState.smartLightOn = on;
        updateSmartLightDisplay(on, false);
        addLog(`Đèn thông minh đã ${on ? 'bật' : 'tắt'}`, on ? 'success' : '');
    } catch (error) {
        console.error('Lỗi Firebase:', error);
        addLog('Lỗi khi điều khiển đèn', 'error');
    }
}


// Test mở cửa bằng AI (gửi lệnh thủ công)
async function testAIDoor() {
    try {
        // CHỈ GỬI LỆNH MỞ
        await database.ref('smarthome/commands/ai_door').set(true);
        addLog('🧪 Đã gửi lệnh test mở cửa AI', 'success');
       
        // Không cần setTimeout set(false) ở đây
    } catch (error) {
        console.error('Lỗi Firebase:', error);
        addLog('Lỗi khi test mở cửa AI', 'error');
    }
}


// Cập nhật trạng thái kết nối
function updateConnectionStatus(connected) {
    const statusDot = document.getElementById('connection-status');
    const statusText = document.getElementById('connection-text');


    if (connected) {
        statusDot.className = 'status-dot online';
        statusText.textContent = 'Đã kết nối';
    } else {
        statusDot.className = 'status-dot offline';
        statusText.textContent = 'Mất kết nối';
    }
}


// Thêm log vào nhật ký
function addLog(message, type = '') {
    const logContainer = document.getElementById('log-container');
    const logItem = document.createElement('div');
    logItem.className = `log-item ${type}`;
   
    const timestamp = new Date().toLocaleTimeString('vi-VN');
    logItem.textContent = `[${timestamp}] ${message}`;
   
    logContainer.insertBefore(logItem, logContainer.firstChild);
   
    // Giới hạn số lượng log
    while (logContainer.children.length > 50) {
        logContainer.removeChild(logContainer.lastChild);
    }
}


// Xử lý lỗi toàn cục
window.addEventListener('error', (event) => {
    addLog(`Lỗi: ${event.message}`, 'error');
});


// Xử lý khi trang bị đóng
window.addEventListener('beforeunload', () => {
    // Có thể gửi lệnh tắt tất cả thiết bị trước khi đóng
});




