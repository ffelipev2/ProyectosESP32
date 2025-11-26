// --- Configuración 3D ---
const scene = new THREE.Scene();
const camera = new THREE.PerspectiveCamera(75, window.innerWidth / window.innerHeight, 0.1, 1000);
const renderer = new THREE.WebGLRenderer({ antialias: true });
renderer.setSize(window.innerWidth, window.innerHeight);
document.body.appendChild(renderer.domElement);

const light = new THREE.DirectionalLight(0xffffff, 1);
light.position.set(5, 10, 7.5);
scene.add(light);
scene.add(new THREE.AmbientLight(0x404040));

const gridHelper = new THREE.GridHelper(10, 10);
scene.add(gridHelper);

camera.position.set(0, 2, 2);
camera.lookAt(new THREE.Vector3(0, 1, 0));

let modelToRotate = null;
let targetQuaternion = new THREE.Quaternion();
let smoothQuaternion = new THREE.Quaternion();

// Offset montaje físico del sensor
const mountingOffset = new THREE.Quaternion();
mountingOffset.setFromEuler(new THREE.Euler(-Math.PI / 2, 0, -Math.PI / 2));

// Offset dinámico (centrado visual)
let dynamicOffset = new THREE.Quaternion();
dynamicOffset.identity();

let lastSensorQuat = null;

// --- Badge estado de calibración ---
const calibStatusEl = document.getElementById("calibStatus");

function updateCalibStatus(level) {
  let text = `Calib: ${level}`;
  let bg = "#666";
  let fg = "#000";

  switch (level) {
    case 0: text += " (Unreliable)"; bg = "#ff4d4f"; fg = "#fff"; break;
    case 1: text += " (Low)"; bg = "#ff9800"; fg = "#000"; break;
    case 2: text += " (Medium)"; bg = "#ffc107"; fg = "#000"; break;
    case 3: text += " (High)"; bg = "#4caf50"; fg = "#fff"; break;
    default: text = "Calib: --"; bg = "#666"; fg = "#000";
  }

  calibStatusEl.textContent = text;
  calibStatusEl.style.backgroundColor = bg;
  calibStatusEl.style.color = fg;
}

// --- Cargar modelo ---
const loader = new THREE.GLTFLoader();
loader.load(
  'https://cdn.jsdelivr.net/gh/ffelipev2/ProyectosESP32@main/2_GemeloDigital_v2_BNO055_CUBONE/pokemon.glb',
  gltf => {
    modelToRotate = gltf.scene;
    modelToRotate.scale.set(1.5, 1.5, 1.5);
    modelToRotate.position.y = 0.8;
    scene.add(modelToRotate);

    const evtSource = new EventSource('/events');
    evtSource.addEventListener('quat', event => {
      const data = JSON.parse(event.data);

      const sensorQuat = new THREE.Quaternion(data.x, data.y, data.z, data.w).normalize();
      lastSensorQuat = sensorQuat;

      // offsets
      targetQuaternion.copy(dynamicOffset).multiply(mountingOffset).multiply(sensorQuat);

      if (typeof data.calib !== "undefined") updateCalibStatus(data.calib);
    });
  }
);

// --- Suavizado inteligente ---
function smartSlerp(fromQ, toQ, slow = 0.08, fast = 0.35) {
  const dot = fromQ.dot(toQ);
  if (Math.abs(dot) < 0.90) fromQ.slerp(toQ, fast);
  else fromQ.slerp(toQ, slow);
}

// --- Animación ---
function animate() {
  requestAnimationFrame(animate);

  smartSlerp(smoothQuaternion, targetQuaternion);

  if (modelToRotate) {
    modelToRotate.setRotationFromQuaternion(smoothQuaternion);
  }

  renderer.render(scene, camera);
}
animate();

// --- Botón Centrar modelo ---
document.getElementById("calibrarModelo").addEventListener("click", () => {
  if (!lastSensorQuat) return;
  const totalQuat = mountingOffset.clone().multiply(lastSensorQuat);
  dynamicOffset.copy(totalQuat.clone().invert());
});

// --- Botón Calibrar IMU ---
document.getElementById("calibrarIMU").addEventListener("click", () => {
  fetch("/calibrate");
});

// --- Resize ---
window.addEventListener("resize", () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});
