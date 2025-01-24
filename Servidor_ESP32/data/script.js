let webSocket = new WebSocket("ws://192.168.4.1:81/");

webSocket.onopen = function () {
  console.log("WebSocket conectado");
};

webSocket.onmessage = function (event) {
  // Si recibimos un número, es la distancia
  if (!isNaN(event.data)) {
    document.getElementById("distance").textContent = `${event.data} cm`;
  } else {
    // Si es texto, mostrarlo como estado del LED
    document.getElementById("ledStatus").textContent = `Estado del LED: ${event.data}`;
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
