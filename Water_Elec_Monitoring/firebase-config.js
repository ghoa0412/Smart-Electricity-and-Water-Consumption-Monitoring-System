// Import the functions you need from the SDKs you need
import { initializeApp } from "firebase/app";
import { getAnalytics } from "firebase/analytics";
// TODO: Add SDKs for Firebase products that you want to use
// https://firebase.google.com/docs/web/setup#available-libraries

// Your web app's Firebase configuration
// For Firebase JS SDK v7.20.0 and later, measurementId is optional
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

// Initialize Firebase
const app = initializeApp(firebaseConfig);
const analytics = getAnalytics(app);