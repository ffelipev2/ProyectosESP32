let webSocket = new WebSocket("ws://192.168.4.1:81/");

webSocket.onopen = function () {
  console.log("WebSocket conectado");
};

webSocket.onmessage = function (event) {
  try {
    const data = JSON.parse(event.data);

    // Actualizar datos de sensores
    if (data.distancia !== undefined) {
      document.getElementById("distance").textContent = `${data.distancia.toFixed(2)} cm`;
    }
    if (data.temperatura !== undefined) {
      document.getElementById("temperature").textContent = `${data.temperatura.toFixed(1)} °C`;
    }
    if (data.humedad !== undefined) {
      document.getElementById("humidity").textContent = `${data.humedad.toFixed(1)} %`;
    }

    // Actualizar estado del LED
    if (data.led !== undefined) {
      const ledStatus = data.led === "on" ? "Encendido" : "Apagado";
      document.getElementById("ledStatus").textContent = `Estado del LED: ${ledStatus}`;
    }
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
