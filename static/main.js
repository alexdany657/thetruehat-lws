var socket = null;

var editWords = {};

window.onload = function() {
    socket = io("wss://" + window.location.host + window.location.pathname);
    var signals = ["sPlayerJoined", "sPlayerLeft", "sMessage", "sYouJoined", "sGameStarted", "sExplanationStarted",
    "sWordExplanationEnded", "sNewWord", "sExplanationEnded", "sWordsToEdit", "sNextTurn", "sGameEnded"];
    for (var i = 0; i < signals.length; ++i) {
        let tmp = signals[i];
        socket.on(tmp, function(data) {
            document.getElementById("r").value = document.getElementById("r").value + tmp + "\n" + JSON.stringify(data) + "\n\n";
            document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
        });
    }
    socket.on("sWordsToEdit", function(data) {
        editWords = data["editWords"];
    });
    socket.onopen(function() {
        document.getElementById("r").value = document.getElementById("r").value + "ready\n\n";
        document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
    });
    socket.onclose(function() {
        document.getElementById("r").value = document.getElementById("r").value + "close\n\n";
        document.getElementById("r").scrollTop = document.getElementById("r").scrollHeight;
    });
    document.querySelector("#submit").onclick = function() {
        socket.emit(document.querySelector("#signal").value, JSON.parse(document.querySelector("#data").value));
    }
    document.querySelector("#login").onclick = function() {
        socket.emit("cJoinRoom", {"username": document.querySelector("#username").value, "time_zone_offset": 0});
    }
    document.querySelector("#logout").onclick = function() {
        socket.emit("cLeaveRoom", {});
    }
    document.querySelector("#start").onclick = function() {
        socket.emit("cStartGame", {});
    }
    document.querySelector("#sready").onclick = function() {
        socket.emit("cSpeakerReady", {});
    }
    document.querySelector("#lready").onclick = function() {
        socket.emit("cListenerReady", {});
    }
    document.querySelector("#endexpl").onclick = function() {
        socket.emit("cEndWordExplanation", {"cause": document.querySelector("#endexplcause").value});
    }
    document.querySelector("#wordsedited").onclick = function() {
        socket.emit("cWordsEdited", {"editWords": editWords});
    }
}
