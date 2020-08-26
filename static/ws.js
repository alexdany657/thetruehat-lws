__SIGNAL_CODES_CLIENT_INCREMENT = 50;

__SIGNAL_CODES = {
    "sMessage": 0,
    "sPlayerJoined": 1,
    "sPlayerLeft": 2,
    "sYouJoined": 3,
    "sGameStarted": 4,
    "sExplanationStarted": 5,
    "sNewWord": 6,
    "sWordExplanationEnded": 7,
    "sExplanationEnded": 8,
    "sWordsToEdit": 9,
    "sNextTurn": 10,
    "sGameEnded": 11,
    "pong": 49,
    "cJoinRoom": __SIGNAL_CODES_CLIENT_INCREMENT + 0,
    "cLeaveRoom": __SIGNAL_CODES_CLIENT_INCREMENT + 1,
    "cStartGame": __SIGNAL_CODES_CLIENT_INCREMENT + 2,
    "cSpeakerReady": __SIGNAL_CODES_CLIENT_INCREMENT + 3,
    "cListenerReady": __SIGNAL_CODES_CLIENT_INCREMENT + 4,
    "cEndWordExplanation": __SIGNAL_CODES_CLIENT_INCREMENT + 5,
    "cWordsEdited": __SIGNAL_CODES_CLIENT_INCREMENT + 6,
    "ping": __SIGNAL_CODES_CLIENT_INCREMENT + 49
}

__PING_INTERVAL = 25000;
__PING_TIME = 5000;

function __str(a) {
    if (typeof(a) != typeof(0)) {
        console.warn("ws: __str: invalid positional argument a: ", a);
        return JSON.stringify(a);
    }
    if (a != a % 100) {
        console.warn(`ws: __str: invalid value of positional argument a: ${a}, a %= 100`);
        a %= 100;
    }
    if (a < 0) {
        console.warn(`ws: __str: negative positional argument a: ${a}, a *= -1`);
        a *= -1;
    }
    var ans = a.toString();
    if (ans.length == 1) {
        ans = "0" + ans;
    }
    return ans;
}

function io(url) {
    var retObj = {};
    retObj.__ws = new WebSocket(url, "lws-tth");
    retObj.__ws.onmessage = function(msg) {
        console.debug(msg);
        var data = msg.data;
        if (data[0] < '0' || data[0] > '9' || data[1] < '0' || data[1] > '9') {
            console.warn("ws: io: onmessage: improper characters on positions 0 and 1, ignoring this signal.", data);
            return;
        }
        var code = data[0] * 10 + data[1] * 1;
        if (code >= __SIGNAL_CODES_CLIENT_INCREMENT) {
            console.warn("ws: io: onmessage: got client signal, ignoring.");
            return;
        }
        if (code == __SIGNAL_CODES["pong"]) {
            retObj.__got_pong = true;
            return;
        }
        data = data.slice(2);
        var signal = "";
        var signals = Object.keys(__SIGNAL_CODES);
        for (var i = 0; i < signals.length; ++i) {
            if (__SIGNAL_CODES[signals[i]] == code) {
                signal = signals[i];
                break;
            }
        }
        if (signal == "") {
            console.warn("ws: io: onmessage: improper signal code:", code, "data:", data);
            return;
        }
        if (!(signal in retObj.__sigListeners)) {
            console.warn("ws: io: onmessage: unhandled signal:", signal, data);
            return;
        }
        for (var i = 0; i < retObj.__sigListeners[signal].length; ++i) {
            retObj.__sigListeners[signal][i](JSON.parse(data));
        }
    }
    retObj.__sigListeners = {};
    retObj.on = function(signal, callback) {
        if (!(signal in retObj.__sigListeners)) {
            retObj.__sigListeners[signal] = [];
        }
        retObj.__sigListeners[signal].push(callback);
    }
    retObj.emit = function(signal, data) {
        if (!(signal in __SIGNAL_CODES)) {
            console.warn("ws: io: emit: improper signal, ignoring.", signal, data);
            return;
        }
        if (__SIGNAL_CODES[signal] < __SIGNAL_CODES_CLIENT_INCREMENT) {
            console.warn("ws: io: emit: trying to emit server signal, ignoring.");
            return;
        }
        retObj.__ws.send(__str(__SIGNAL_CODES[signal]) + JSON.stringify(data));
    }
    retObj.onclose = function(callback) {
        retObj.__ws.onclose = callback;
    }
    retObj.onerror = function(callback) {
        retObj.__ws.onerror = callback;
    }
    retObj.onopen = function(callback) {
        retObj.__ws.onopen = callback;
    }
    window.setInterval(function(retObj) {
        retObj.__got_pong = false;
        retObj.__ws.send(__str(__SIGNAL_CODES["ping"]));
        window.setTimeout(function(retObj) {
            if (!retObj.__got_pong) {
                retObj.__ws.close(1000);
            }
        }, __PING_TIME, retObj);
    }, __PING_INTERVAL, retObj);
    return retObj;
}
