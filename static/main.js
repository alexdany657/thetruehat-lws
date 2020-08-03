var socket;

window.onload = function() {
    socket = io("ws://localhost:5000");
    var signals = ["sPlayerJoined", "sPlayerLeft", "sMessage", "sYouJoined"];
    for (var i = 0; i < signals.length; ++i) {
        let tmp = signals[i];
        socket.on(tmp, function(data) {
            document.getElementById("r").value = document.getElementById("r").value + tmp + "\n" + JSON.stringify(data) + "\n\n";
            document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
        });
    }
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
