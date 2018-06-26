var getJSON = function (url, callback) {
    var xhr = new XMLHttpRequest();
    xhr.open('GET', url, true);
    xhr.responseType = 'json';
    xhr.onload = function () {
        var status = xhr.status;
        if (status == 200) {
            callback(null, xhr.response);
        } else {
            callback(status);
        }
    };
    xhr.send();
};

function OnLoad() {
    getJSON('networks', function (err, data) {
        if (err == null) {
            var options = '';
            for (i = 0; i < data.length; i++) {
                options += '<option value="' + data[i].ssid + '" />';
            }
            document.getElementById('ssids').innerHTML = options;
        }
    });
}