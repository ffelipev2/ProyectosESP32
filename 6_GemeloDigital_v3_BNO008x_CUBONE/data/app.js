// --- Escena básica ---
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

const mountingOffset = new THREE.Quaternion();
mountingOffset.setFromEuler(new THREE.Euler(-Math.PI / 2, 0, -Math.PI / 2));

let dynamicOffset = new THREE.Quaternion();
dynamicOffset.identity();

let lastSensorQuat = null;

// --- Carga del modelo 3D ---
const loader = new THREE.GLTFLoader();
loader.load('https://cdn.jsdelivr.net/gh/ffelipev2/ProyectosESP32@main/2_GemeloDigital_v2_BNO055_CUBONE/pokemon.glb', (gltf) => {
  modelToRotate = gltf.scene;
  modelToRotate.scale.set(1.5, 1.5, 1.5);
  modelToRotate.position.y = 0.8;
  scene.add(modelToRotate);

  // --- Conexión SSE ---
  const evtSource = new EventSource('/events');
  evtSource.addEventListener('quat', (event) => {
    const data = JSON.parse(event.data);
    const sensorQuat = new THREE.Quaternion(data.x, data.y, data.z, data.w).normalize();
    lastSensorQuat = sensorQuat;

    targetQuaternion.copy(dynamicOffset).multiply(mountingOffset).multiply(sensorQuat);
  });
});

// --- Animación principal ---
function animate() {
  requestAnimationFrame(animate);
  smoothQuaternion.slerp(targetQuaternion, 0.1);

  if (modelToRotate) {
    modelToRotate.setRotationFromQuaternion(smoothQuaternion);

    const manualX = parseFloat(document.getElementById('rotX').value || 0);
    const manualY = parseFloat(document.getElementById('rotY').value || 0);
    const manualZ = parseFloat(document.getElementById('rotZ').value || 0);
    const ganancia = parseFloat(document.getElementById('gananciaInput').value || 1.0);

    const manualEuler = new THREE.Euler(manualX * ganancia, manualY * ganancia, manualZ * ganancia, 'ZYX');
    const manualQuat = new THREE.Quaternion().setFromEuler(manualEuler);
    modelToRotate.quaternion.multiply(manualQuat);
  }

  renderer.render(scene, camera);
}
animate();

// --- UI ---
document.querySelectorAll('input[type="number"]').forEach(input => {
  input.addEventListener('input', updateUI);
});

function updateUI(event) {
  const { id, value } = event.target;
  const span = document.getElementById(`val${id.charAt(3).toUpperCase() + id.slice(4)}`);
  span.textContent = Number(value).toFixed(2);
}

// --- Botón copiar valores ---
document.getElementById('anotar').addEventListener('click', () => {
  const fx = parseFloat(document.getElementById('rotX').value).toFixed(4);
  const fy = parseFloat(document.getElementById('rotY').value).toFixed(4);
  const fz = parseFloat(document.getElementById('rotZ').value).toFixed(4);
  alert(`Valores copiados:\nX: ${fx}\nY: ${fy}\nZ: ${fz}`);
});

// --- Botón calibrar ---
document.getElementById('calibrar').addEventListener('click', () => {
  if (lastSensorQuat) {
    const currentTotalQuat = mountingOffset.clone().multiply(lastSensorQuat);
    const inverseTotal = currentTotalQuat.clone().invert();
    dynamicOffset.copy(inverseTotal);
    console.log("✅ Calibración aplicada correctamente.");
  } else {
    console.log("Esperando datos del sensor para calibrar...");
  }
});

// --- Redimensionar ventana ---
window.addEventListener('resize', () => {
  camera.aspect = window.innerWidth / window.innerHeight;
  camera.updateProjectionMatrix();
  renderer.setSize(window.innerWidth, window.innerHeight);
});
