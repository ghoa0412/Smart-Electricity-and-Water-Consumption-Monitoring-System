import { initializeApp } from "https://www.gstatic.com/firebasejs/10.12.2/firebase-app.js";
import {
getAuth,
GoogleAuthProvider,
signInWithPopup,
signOut,
onAuthStateChanged,
updateProfile
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-auth.js";
import {
getDatabase,
ref,
set,
get,
onValue
} from "https://www.gstatic.com/firebasejs/10.12.2/firebase-database.js";

const firebaseConfig = {
apiKey: "AIzaSyCIadBXM4pFg880vCDWUsv098FuzjVvNRU",
authDomain: "project-a664b.firebaseapp.com",
databaseURL: "https://project-a664b-default-rtdb.firebaseio.com",
projectId: "project-a664b",
storageBucket: "project-a664b.firebasestorage.app",
messagingSenderId: "562942830014",
appId: "1:562942830014:web:6f974d995c1d1351fd85ef",
measurementId: "G-ERFJWDQETQ"
};

const app = initializeApp(firebaseConfig);
const auth = getAuth(app);
const db = getDatabase(app);
const provider = new GoogleAuthProvider();

provider.setCustomParameters({
prompt: 'select_account'
});

let uid = "";
let c1 = null;
let c2 = null;

// DOM elements
const loginScreen = document.getElementById("loginScreen");
const dashboard = document.getElementById("dashboard");
const tabsBar = document.getElementById("tabsBar");
const adminPanel = document.getElementById("admin");
const monitorPanel = document.getElementById("monitor");
const tabAdminBtn = document.getElementById("tabAdmin");
const tabMonitorBtn = document.getElementById("tabMonitor");
const logoutBtn = document.getElementById("logoutBtn");
const loginBtn = document.getElementById("loginBtn");
const editInfoBtn = document.getElementById("editInfoBtn");
const loginError = document.getElementById("loginError");

// Modal elements
const editModal = document.getElementById("editModal");
const firstLoginModal = document.getElementById("firstLoginModal");
const editNameInput = document.getElementById("editNameInput");
const editAddressInput = document.getElementById("editAddressInput");
const editPhoneInput = document.getElementById("editPhoneInput");
const saveEditBtn = document.getElementById("saveEditBtn");
const closeModalBtn = document.getElementById("closeModalBtn");
const saveFirstInfoBtn = document.getElementById("saveFirstInfoBtn");
const firstAddressInput = document.getElementById("firstAddressInput");
const firstPhoneInput = document.getElementById("firstPhoneInput");

console.log("Main.js đã được load thành công!");

// Hàm lấy nhãn ngày cho 7 ngày gần nhất
function getLast7DaysFullLabels() {
const labels = [];
const today = new Date();
const thu = ["CN", "T2", "T3", "T4", "T5", "T6", "T7"];
for (let i = 6; i >= 0; i--) {
const date = new Date(today);
date.setDate(today.getDate() - i);
const day = date.getDate().toString().padStart(2, '0');
const month = (date.getMonth() + 1).toString().padStart(2, '0');
const thuIndex = date.getDay();
labels.push(`${thu[thuIndex]} ${day}/${month}`);
}
return labels;
}

// Hàm hiển thị tab
window.showTab = function(tab) {
console.log("Chuyển sang tab:", tab);
if(!adminPanel || !monitorPanel) return;

if(tab === "admin"){
adminPanel.style.display = "flex";
monitorPanel.style.display = "none";
if(tabAdminBtn) tabAdminBtn.classList.add("active");
if(tabMonitorBtn) tabMonitorBtn.classList.remove("active");
} else {
adminPanel.style.display = "none";
monitorPanel.style.display = "flex";
if(tabMonitorBtn) tabMonitorBtn.classList.add("active");
if(tabAdminBtn) tabAdminBtn.classList.remove("active");
// Vẽ lại biểu đồ khi chuyển sang tab monitor
setTimeout(() => {
if(window.chartData) {
drawChart(window.chartData);
}
}, 100);
}
};

// Mở modal chỉnh sửa
function openEditModal() {
console.log("Mở modal chỉnh sửa");
if(!editModal) return;

const nameSpan = document.getElementById("name");
const addressSpan = document.getElementById("address");
const phoneSpan = document.getElementById("phone");

if(editNameInput) editNameInput.value = nameSpan ? (nameSpan.innerText !== "---" ? nameSpan.innerText : "") : "";
if(editAddressInput) editAddressInput.value = addressSpan ? (addressSpan.innerText !== "---" ? addressSpan.innerText : "") : "";
if(editPhoneInput) editPhoneInput.value = phoneSpan ? (phoneSpan.innerText !== "---" ? phoneSpan.innerText : "") : "";

editModal.classList.add("show");
}

// Đóng modal
function closeModal() {
if(editModal) editModal.classList.remove("show");
if(firstLoginModal) firstLoginModal.classList.remove("show");
}

// Lưu thông tin chỉnh sửa
window.saveEditInfo = async function() {
console.log("Đang lưu thông tin chỉnh sửa...");
const user = auth.currentUser;
if(!user) return;

const newName = editNameInput ? editNameInput.value.trim() : "";
const newAddress = editAddressInput ? editAddressInput.value.trim() : "";
const newPhone = editPhoneInput ? editPhoneInput.value.trim() : "";

if(!newName || !newAddress || !newPhone){
alert("Vui lòng nhập đầy đủ thông tin!");
return;
}

// Cập nhật tên trong Firebase Auth
if(newName !== user.displayName) {
try {
await updateProfile(user, { displayName: newName });
console.log("Đã cập nhật tên trong Auth");
} catch(err) {
console.error("Lỗi cập nhật tên:", err);
}
}

// Cập nhật thông tin trong Realtime Database
await set(ref(db, "users/" + uid), {
name: newName,
email: user.email,
address: newAddress,
phone: newPhone
});

console.log("Đã lưu thông tin thành công");
closeModal();
loadData();
};

// Lưu thông tin lần đầu
window.saveFirstInfo = async function() {
console.log("Đang lưu thông tin lần đầu...");
const user = auth.currentUser;
if(!user) return;

const address = firstAddressInput ? firstAddressInput.value.trim() : "";
const phone = firstPhoneInput ? firstPhoneInput.value.trim() : "";

if(!address || !phone){
alert("Vui lòng nhập đầy đủ địa chỉ và số điện thoại!");
return;
}

await set(ref(db, "users/" + uid), {
name: user.displayName,
email: user.email,
address: address,
phone: phone
});

console.log("Đã lưu thông tin thành công");
closeModal();
if(tabsBar) tabsBar.style.display = "flex";
loadData();
window.showTab("admin");
};

// Load dữ liệu realtime
function loadData(){
console.log("Đang load dữ liệu từ Firebase...");

const userRef = ref(db, "users/" + uid);
onValue(userRef, (snap) => {
const d = snap.val();
if(!d) return;
const nameEl = document.getElementById("name");
const emailEl = document.getElementById("email");
const addressEl = document.getElementById("address");
const phoneEl = document.getElementById("phone");
if(nameEl) nameEl.innerText = d.name || "---";
if(emailEl) emailEl.innerText = d.email || "---";
if(addressEl) addressEl.innerText = d.address || "---";
if(phoneEl) phoneEl.innerText = d.phone || "---";
console.log("Đã cập nhật thông tin user");
});

const hienTaiRef = ref(db, "ThongSo/HienTai");
onValue(hienTaiRef, (snap) => {
const d = snap.val();
if(!d) return;

// HIỂN THỊ NHIỀU SỐ THẬP PHÂN CHO ĐIỆN (WH -> kWh)
let dienKwh = 0;
let nuocM3 = 0;

if(d.DienNang_Wh !== undefined) {
    // Giữ nguyên 6 số thập phân cho kWh
    dienKwh = (d.DienNang_Wh / 1000).toFixed(6);
}
if(d.TongNuoc_L !== undefined) {
    // Giữ nguyên 6 số thập phân cho m³
    nuocM3 = (d.TongNuoc_L / 1000).toFixed(6);
}

const electricEl = document.getElementById("electric");
const waterEl = document.getElementById("water");
const totalMoneyEl = document.getElementById("totalMoney");

if(electricEl) electricEl.innerHTML = dienKwh + " kWh";
if(waterEl) waterEl.innerHTML = nuocM3 + " m³";

// Tính tổng tiền với độ chính xác cao
const total = (parseFloat(dienKwh) * 3000) + (parseFloat(nuocM3) * 7000);
if(totalMoneyEl) totalMoneyEl.innerHTML = total.toLocaleString("vi-VN", {
    minimumFractionDigits: 2,
    maximumFractionDigits: 2
}) + " VNĐ";

console.log("Đã cập nhật chỉ số điện nước: Điện=" + dienKwh + " kWh, Nước=" + nuocM3 + " m³");
});

const weekRef = ref(db, "ThongSo/7Ngay");
onValue(weekRef, (snap) => {
const data = snap.val();
console.log("Dữ liệu 7 ngày nhận được:", data);
if(data && data.electric && data.water){
// Đảm bảo dữ liệu hiển thị nhiều số thập phân
const formattedData = {
    electric: data.electric.map(v => parseFloat(v).toFixed(6)),
    water: data.water.map(v => parseFloat(v).toFixed(6))
};
window.chartData = formattedData;
drawChart(formattedData);
} else {
// Dữ liệu mẫu với nhiều số thập phân (giả lập WH và Lít)
const demoData = {
    electric: [3.123456, 4.234567, 5.345678, 4.456789, 6.567890, 5.678901, 6.789012],
    water: [0.123456, 0.234567, 0.345678, 0.456789, 0.567890, 0.678901, 0.789012]
};
window.chartData = demoData;
drawChart(demoData);
}
});
}

// Vẽ biểu đồ (HIỂN THỊ NHIỀU SỐ THẬP PHÂN TRONG TOOLTIP)
function drawChart(data){
console.log("Đang vẽ biểu đồ với dữ liệu:", data);
const labels = getLast7DaysFullLabels();
const electricCanvas = document.getElementById("electricChart");
const waterCanvas = document.getElementById("waterChart");

if(!electricCanvas || !waterCanvas){
console.error("Không tìm thấy canvas elements");
return;
}

const electricCtx = electricCanvas.getContext("2d");
const waterCtx = waterCanvas.getContext("2d");

if(!electricCtx || !waterCtx){
console.error("Không thể lấy context của canvas");
return;
}

if(c1) {
try { c1.destroy(); } catch(e) { console.log(e); }
c1 = null;
}
if(c2) {
try { c2.destroy(); } catch(e) { console.log(e); }
c2 = null;
}

c1 = new Chart(electricCtx, {
type: "line",
data: {
labels: labels,
datasets: [{
label: "Điện (kWh)",
data: data.electric,
borderColor: "#f97316",
backgroundColor: "rgba(249,115,22,0.1)",
tension: 0.3,
fill: true,
pointBackgroundColor: "#ea580c",
pointRadius: 5,
pointHoverRadius: 7
}]
},
options: { 
responsive: true, 
maintainAspectRatio: true,
plugins: {
tooltip: {
callbacks: {
label: function(context) {
// HIỂN THỊ 6 SỐ THẬP PHÂN TRONG TOOLTIP
return `Điện: ${parseFloat(context.raw).toFixed(6)} kWh`;
}
}
},
legend: { position: 'top' }
}
}
});

c2 = new Chart(waterCtx, {
type: "line",
data: {
labels: labels,
datasets: [{
label: "Nước (m³)",
data: data.water,
borderColor: "#0e7c9e",
backgroundColor: "rgba(14,124,158,0.1)",
tension: 0.3,
fill: true,
pointBackgroundColor: "#0b5e7a",
pointRadius: 5,
pointHoverRadius: 7
}]
},
options: { 
responsive: true, 
maintainAspectRatio: true,
plugins: {
tooltip: {
callbacks: {
label: function(context) {
// HIỂN THỊ 6 SỐ THẬP PHÂN TRONG TOOLTIP
return `Nước: ${parseFloat(context.raw).toFixed(6)} m³`;
}
}
},
legend: { position: 'top' }
}
}
});

console.log("Biểu đồ đã được vẽ thành công");
}

// XỬ LÝ ĐĂNG NHẬP
if(loginBtn) {
loginBtn.onclick = async () => {
console.log("Người dùng click đăng nhập");
if(loginError) loginError.innerText = "Đang đăng nhập...";
try {
const result = await signInWithPopup(auth, provider);
console.log("Đăng nhập thành công:", result.user.email);
if(loginError) loginError.innerText = "";
} catch (err) {
console.error("Lỗi đăng nhập chi tiết:", err);
let errorMsg = "";
switch(err.code){
case 'auth/popup-blocked':
errorMsg = "Trình duyệt đã chặn popup. Vui lòng cho phép popup và thử lại.";
break;
case 'auth/popup-closed-by-user':
errorMsg = "Bạn đã đóng cửa sổ đăng nhập. Vui lòng thử lại.";
break;
case 'auth/unauthorized-domain':
errorMsg = "Domain chưa được cấp phép. Vui lòng thêm domain này vào Firebase Console.";
break;
case 'auth/network-request-failed':
errorMsg = "Lỗi kết nối mạng. Kiểm tra internet của bạn.";
break;
default:
errorMsg = "Đăng nhập thất bại: " + err.message;
}
if(loginError) loginError.innerText = errorMsg;
alert(errorMsg);
}
};
}

// Đăng xuất
if(logoutBtn) {
logoutBtn.onclick = async () => {
console.log("Người dùng đăng xuất");
await signOut(auth);
if(c1) {
try { c1.destroy(); } catch(e) {}
c1 = null;
}
if(c2) {
try { c2.destroy(); } catch(e) {}
c2 = null;
}
location.reload();
};
}

// Sự kiện các nút
if(editInfoBtn) editInfoBtn.onclick = () => openEditModal();
if(saveEditBtn) saveEditBtn.onclick = () => window.saveEditInfo();
if(closeModalBtn) closeModalBtn.onclick = () => closeModal();
if(saveFirstInfoBtn) saveFirstInfoBtn.onclick = () => window.saveFirstInfo();
if(tabAdminBtn) tabAdminBtn.onclick = () => window.showTab("admin");
if(tabMonitorBtn) tabMonitorBtn.onclick = () => window.showTab("monitor");

// Click outside modal để đóng
window.onclick = (event) => {
if(editModal && event.target === editModal) closeModal();
if(firstLoginModal && event.target === firstLoginModal) closeModal();
};

// Theo dõi trạng thái đăng nhập
onAuthStateChanged(auth, async (user) => {
console.log("Trạng thái đăng nhập thay đổi:", user ? "Đã đăng nhập" : "Chưa đăng nhập");
if(user){
uid = user.uid;
if(loginScreen) loginScreen.style.display = "none";
if(dashboard) dashboard.style.display = "block";

const snap = await get(ref(db, "users/" + uid));
if(!snap.exists()){
console.log("Người dùng mới, hiển thị modal nhập thông tin");
if(tabsBar) tabsBar.style.display = "none";
if(firstLoginModal) firstLoginModal.classList.add("show");
} else {
console.log("Người dùng cũ, load dữ liệu");
if(tabsBar) tabsBar.style.display = "flex";
loadData();
window.showTab("admin");
}
} else {
if(loginScreen) loginScreen.style.display = "flex";
if(dashboard) dashboard.style.display = "none";
if(c1) {
try { c1.destroy(); } catch(e) {}
c1 = null;
}
if(c2) {
try { c2.destroy(); } catch(e) {}
c2 = null;
}
}
});