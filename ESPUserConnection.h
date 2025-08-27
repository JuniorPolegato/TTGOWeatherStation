#ifndef ESP_USER_CONNECTION_H
#define ESP_USER_CONNECTION_H

#define PROJECT_NAME "TTGO Weather Station"

// Uncomment the follow lines to request a autentication
#define AUTHENTICATION_USERNAME "Junior"
#define AUTHENTICATION_PASSWORD "Polegato"

// Uncomment the follow line to permit OTA update
#define OTA_UPDATE 1

// Uncomment this if you use custom user_config.html to get extra user data.
// You need to provide the function "custom_user_request_data(request)"
// into ESPUserConnection.cpp (see exemple there), also is needed to create
// custom_user_config.html based on user_config.html (see diffs between these)
// and upload this file to ESP at first boot. Create and upload index.html.
// You also need to customize sendfiles_html and go_back_html bellow.
#define CUSTOM_USER_REQUEST_DATA

// Uncomment and adjust next lines to output info to TFT
#define OUTPUT_IS_TFT
#define TFT_BACKGROUND TFT_DARKGREEN
#define TFT_TEXT_COLOR TFT_YELLOW

// Uncomment this to see the IP to connect to on Bluetooh name
//#define IP_ON_BLUETOOTH_NAME "Weather Station"

#ifdef CUSTOM_USER_REQUEST_DATA
const char sendfiles_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en-us">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>TTGO Weather Station - by Junior Polegato</title>
</head>
<body>
    <h1><a href="/">TTGO Weather Station - by Junior Polegato</a></h1>
    <h2>Select and send files to TTGO Weather Station board<h2>
    <h3>Please, pay attention! You can destroy data and crash your Weather Station.<h3>
    <form action="/send_file" method="POST" enctype="multipart/form-data">
        <input type="file" name="file" id="file">
        <button type="submit">Send</button>
    </form>
</body>
</html>
)===";

const char go_back_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en-us">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="1; url=/">
    <title>TTGO Weather Station - by Junior Polegato</title>
</head>
<body>
    <h1>TTGO Weather Station - by Junior Polegato</h1>
</body>
</html>
)===";
#else
const char sendfiles_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en-us">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>ESP User Connection - by Junior Polegato</title>
</head>
<body>
    <h1>ESP User Connection - by Junior Polegato</h1>
    <h2>Select and send files to ESP board<h2>
    <h3>Please, pay attention! You can destroy data and crash your ESP.<h3>
    <form action="/send_file" method="POST" enctype="multipart/form-data">
        <input type="file" name="file" id="file">
        <button type="submit">Send</button>
    </form>
</body>
</html>
)===";

const char go_back_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en-us">
<head>
    <meta charset="UTF-8">
    <meta http-equiv="refresh" content="1; url=/">
    <title>ESP User Connection - by Junior Polegato</title>
</head>
<body>
    <h1>ESP User Connection - by Junior Polegato</h1>
</body>
</html>
)===";
#endif  // CUSTOM_USER_REQUEST_DATA

#ifdef OTA_UPDATE  // https://lastminuteengineers.com/esp32-ota-web-updater-arduino-ide/
const char ota_index_html[] PROGMEM = R"===(
<!DOCTYPE html>
<html lang="en-us">
<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0">
    <title>TTGO Weather Station - by Junior Polegato</title>
    <script src='https://ajax.googleapis.com/ajax/libs/jquery/3.2.1/jquery.min.js'></script>
    <style>
        #file-input,input {
            width: 100%;
            height: 44px;
            border-radius: 4px;
            margin: 10px auto;
            font-size: 15px
        }
        input {
            background: #f1f1f1;
            border: 0;
            padding: 0 15px;
        }
        body {
            background: #3498db;
            font-family: sans-serif;
            font-size: 14px;
            color: #777;
        }
        #file-input {
            padding: 0;
            border: 1px solid #ddd;
            line-height: 44px;
            text-align: left;
            display: block;
            cursor: pointer;
        }
        #bar, #prgbar {
            background-color: #f1f1f1;
            border-radius: 10px;
        }
        #bar {
            background-color: #3498db;
            width: 0%;
            height: 10px;
        }
        form {
            background: #fff;
            max-width: 50%;
            margin: 75px auto;
            padding: 30px;
            border-radius: 5px;
            text-align: center;
        }
        .btn {
            background: darkgreen; /* #3498db */
            color: yellow; /* #fff */
            cursor: pointer;
        }
    </style>
</head>
<body>
    <form method='POST' action='#' enctype='multipart/form-data' id='upload_form'>
        <input type='file' name='update' id='file' onchange='sub(this)' style=display:none>
        <label id='file-input' for='file'>&nbsp; &nbsp; Choose file...</label>
        <input type='submit' class=btn value='Update'>
        <br><br>
        <div id='prg'></div>
        <br><div id='prgbar'><div id='bar'>
        </div></div><br>
    </form>
    <script>
        function sub(obj){
            var fileName = obj.value.split('\\');
            document.getElementById('file-input').innerHTML = '&nbsp; &nbsp; '+ fileName[fileName.length-1];
        };
        $('form').submit(function(e){
            e.preventDefault();
            var form = $('#upload_form')[0];
            var data = new FormData(form);
            $.ajax({
                url: '/update',
                type: 'POST',
                data: data,
                contentType: false,
                processData:false,
                xhr: function() {
                    var xhr = new window.XMLHttpRequest();
                    xhr.upload.addEventListener('progress', function(evt) {
                        if (evt.lengthComputable) {
                            var per = evt.loaded / evt.total;
                            $('#prg').html('progress: ' + Math.round(per*100) + '%');
                            $('#bar').css('width',Math.round(per*100) + '%');
                        }
                    }, false);
                    return xhr;
                },
                success:function(d, s) {
                    console.log('success!')
                },
                error: function (a, b, c) {
                }
            });
        });
    </script>
</body>
</html>
)===";
#endif // OTA_UPDATE

#ifdef OUTPUT_IS_TFT
bool connect_wifi(void *tft, bool force_ap_mode=false, bool show_connected_ip=true);
#else
bool connect_wifi(bool force_ap_mode=false, bool show_connected_ip=true);
#endif  // OUTPUT_IS_TFT

#endif // ESP_USER_CONNECTION_H
