document.addEventListener("DOMContentLoaded", () => {
  console.log("Interfaz WiFi cargada correctamente");

  const form = document.querySelector("form");
  form.addEventListener("submit", () => {
    alert("Credenciales enviadas, reinicia el ESP32.");
  });
});

