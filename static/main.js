var socket;

window.onload = function() {
    socket = io("ws://localhost:5000");
    socket.on("sPlayerJoined", function(msg) {
        document.getElementById("r").value = document.getElementById("r").value + JSON.stringify(msg) + "\n";
        document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
    });
    socket.on("sPlayerLeft", function(msg) {
        document.getElementById("r").value = document.getElementById("r").value + JSON.stringify(msg) + "\n";
        document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
    });
    socket.on("sMessage", function(msg) {
        document.getElementById("r").value = document.getElementById("r").value + msg.msg + "\n";
        document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
    })
    document.querySelector("#submit").onclick = function() {
        socket.emit(document.querySelector("#signal").value, JSON.parse(document.querySelector("#data").value));
    }
    document.querySelector("#login").onclick = function() {
        socket.emit("cJoinRoom", {"username": document.querySelector("#username").value, "time_zone_offset": 0});
    }
    document.querySelector("#logout").onclick = function() {
        socket.emit("cLeaveRoom", {});
    }
}
