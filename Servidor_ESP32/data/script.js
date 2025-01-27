let webSocket = new WebSocket("ws://192.168.4.1:81/");

webSocket.onopen = function () {
  console.log("WebSocket conectado");
};

webSocket.onmessage = function (event) {
  try {
    const data = JSON.parse(event.data);
    document.getElementById("distance").textContent = `${data.distancia.toFixed(2)} cm`;
    document.getElementById("temperature").textContent = `${data.temperatura.toFixed(1)} °C`;
    document.getElementById("humidity").textContent = `${data.humedad.toFixed(1)} %`;
  } catch (error) {
    console.error("Error al procesar datos WebSocket:", error);
  }
};

webSocket.onclose = function () {
  console.log("WebSocket desconectado");
};

function toggleLED(action) {
  if (webSocket.readyState === WebSocket.OPEN) {
    webSocket.send(action);
  } else {
    console.error("WebSocket no está conectado");
  }
}
