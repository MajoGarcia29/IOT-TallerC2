document.getElementById("wifi-form").addEventListener("submit", function(event) {
    event.preventDefault();

    let ssid = document.getElementById("ssid").value;
    let password = document.getElementById("password").value;

    document.getElementById("status").innerText = "enviando datos...";

});
