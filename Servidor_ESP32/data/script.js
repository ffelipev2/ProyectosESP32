let webSocket = new WebSocket("ws://192.168.4.1:81/");

webSocket.onopen = function () {
  console.log("WebSocket conectado");
};

webSocket.onmessage = function (event) {
  try {
    console.log("Mensaje recibido:", event.data);  // Log para depurar

    const data = JSON.parse(event.data);

    // Actualizar datos de sensores con parseo seguro
    if (data.distancia !== undefined) {
      const dist = parseFloat(data.distancia);
      if (!isNaN(dist)) {
        document.getElementById("distance").textContent = `${dist.toFixed(2)} cm`;
      } else {
        document.getElementById("distance").textContent = `Dato inválido`;
      }
    }

    if (data.temperatura !== undefined) {
      const temp = parseFloat(data.temperatura);
      if (!isNaN(temp)) {
        document.getElementById("temperature").textContent = `${temp.toFixed(1)} °C`;
      } else {
        document.getElementById("temperature").textContent = `Dato inválido`;
      }
    }

    if (data.humedad !== undefined) {
      const hum = parseFloat(data.humedad);
      if (!isNaN(hum)) {
        document.getElementById("humidity").textContent = `${hum.toFixed(1)} %`;
      } else {
        document.getElementById("humidity").textContent = `Dato inválido`;
      }
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
