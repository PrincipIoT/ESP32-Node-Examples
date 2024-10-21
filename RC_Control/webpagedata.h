const char index_html[] PROGMEM = R"rawliteral(


<!DOCTYPE html>
<html lang="en">

<head>
    <meta charset="UTF-8">
    <meta name="viewport" content="width=device-width, initial-scale=1.0, user-scalable=no">
    <meta name="apple-mobile-web-app-capable" content="yes">
    <meta name="apple-mobile-web-app-status-bar-style" content="black-translucent">
    <meta name="mobile-web-app-capable" content="yes">
    <title>Drone Control Interface</title>
    <style>
        body,
        html {
            margin: 0;
            padding: 0;
            width: 100%;
            overflow: hidden;
            background-color: #333;
        }

        canvas {
            display: block;
            touch-action: none;
        }
    </style>
    <script type="application/manifest+json">
        {
          "name": "Drone Control Interface",
          "short_name": "Drone Control",
          "start_url": "/index.html",
          "display": "standalone",
          "background_color": "#333333",
          "theme_color": "#333333",
          "icons": [
            {
              "src": "icon-192x192.png",
              "sizes": "192x192",
              "type": "image/png"
            },
            {
              "src": "icon-512x512.png",
              "sizes": "512x512",
              "type": "image/png"
            }
          ]
        }
        </script>
</head>

<body>
    <script>

        if ('serviceWorker' in navigator) {
            window.addEventListener('load', function() {
                navigator.serviceWorker.register('/sw.js').then(function(registration) {
                    console.log('ServiceWorker registration successful with scope: ', registration.scope);
                }, function(err) {
                    console.log('ServiceWorker registration failed: ', err);
                });
            });
        }

        // Inline Service Worker
        const swContent = `
            const CACHE_NAME = 'drone-control-cache-v1';
            const urlsToCache = [
                '/',
                '/index.html',
                '/icon-192x192.png',
                '/icon-512x512.png'
            ];

            self.addEventListener('install', function(event) {
                event.waitUntil(
                    caches.open(CACHE_NAME)
                        .then(function(cache) {
                            return cache.addAll(urlsToCache);
                        })
                );
            });

            self.addEventListener('fetch', function(event) {
                event.respondWith(
                    caches.match(event.request)
                        .then(function(response) {
                            if (response) {
                                return response;
                            }
                            return fetch(event.request);
                        })
                );
            });
        `;

        const blob = new Blob([swContent], {type: 'text/javascript'});
        const swUrl = URL.createObjectURL(blob);

        if ('serviceWorker' in navigator) {
            navigator.serviceWorker.register(swUrl)
                .then(function(registration) {
                    console.log('Service Worker registered with scope:', registration.scope);
                })
                .catch(function(error) {
                    console.log('Service Worker registration failed:', error);
                });
        }

        var socket;
        const updateRate = 30; //Hz

        function startUpdateLoop() {
            setInterval(() => {
                send_rc_packet();
            }, [1000 / updateRate])
        }

        function init() {
            socket = new WebSocket("ws://" + window.location.host + "/ws");
            socket.onopen = function () {
                // console.log("WebSocket connected");
                startUpdateLoop();
            };
            socket.onmessage = function (event) {
                // console.log("Received: " + event.data);
            };
        }
        function sendMessage(message) {
            var msg = new Uint8Array(message);
            console.log("Sending raw packet: ");
            console.log(msg);
            socket.send(msg);
        }
        function crc8_dvb_s2(crc, a) {
            crc ^= a
            for (var i = 0; i < 8; i++) {
                if (crc & 0x80) {
                    crc = ((crc << 1) ^ 0xD5) % 256;
                } else {
                    crc = (crc << 1) % 256;
                }
            }
            return crc;
        }
        function calculate_checksum(msg, payload_len) {
            var checksum = 0;

            for (var i = 3; i < payload_len + 8; i++) {
                checksum = crc8_dvb_s2(checksum, msg[i]);
            }
            return checksum;
        }

        function intToBytesLittleEndian(val) {
            if (val < 0 || val > 0xFFFF) {
                throw new RangeError("Value should be between 0 and 65535 (0xFFFF)");
            }
            const bytes = new Uint8Array(2);
            bytes[0] = val & 0xFF;        // Least significant byte
            bytes[1] = (val >> 8) & 0xFF; // Most significant byte
            return bytes;
        }

        // Example usage:
        const val = 12345; // Your integer value
        const bytes = intToBytesLittleEndian(val);
        // console.log(bytes); // Uint8Array [ 57, 48 ]


        function make_packet(rc_inputs) {
            console.log("Rc inputs: " + rc_inputs);
            num_inputs = rc_inputs.length;
            data = [0x24, 0x58, 0x3c, 0x00, 0xc8, 0x00, num_inputs * 2, 0x00];
            for (var i = 0; i < num_inputs; i++) {
                packed = intToBytesLittleEndian(rc_inputs[i]);
                data.push(packed[0]);
                data.push(packed[1]);
            }
            data.push(calculate_checksum(data, num_inputs * 2));
            return data
        }


        function send_rc_packet() {
            console.log("Packet");
            var a = parseInt((rightJoystick.normalizedX * 1000) + 1000);
            var e = parseInt((rightJoystick.normalizedY * 1000) + 1000);
            var r = parseInt((leftJoystick.normalizedX * 1000) + 1000);
            var t = parseInt((leftJoystick.normalizedY * 1000) + 1000);
            var arm = (armToggle.armed * 1000) + 1000;
            var mode = (modeToggle.mode * 500) + 1000;

            var packet = make_packet([a, e, r, t, arm, mode]);
            sendMessage(packet)
        }

        init();

        // Create canvas and context
        const canvas = document.createElement('canvas');
        document.body.appendChild(canvas);
        const ctx = canvas.getContext('2d');

        // Interface elements
        let leftJoystick, rightJoystick, armToggle, modeToggle;

        // Set canvas size and adjust interface elements
        function resizeCanvas() {
            canvas.width = window.innerWidth;
            canvas.height = window.innerHeight;

            const minDimension = Math.min(canvas.width, canvas.height);
            const rad_factor = (canvas.width > canvas.height) ? 0.3 : 0.15;
            const joystickRadius = minDimension * rad_factor;
            const toggleWidth = minDimension * 0.3;
            const toggleHeight = minDimension * 0.16;

            leftJoystick = {
                x: joystickRadius * 1.5,
                y: canvas.height - joystickRadius * 1.5,
                radius: joystickRadius,
                active: false,
                knobX: joystickRadius * 1.5,
                knobY: canvas.height - joystickRadius * 0.5,
                normalizedX: 0.5,
                normalizedY: 0.5,
            };

            rightJoystick = {
                x: canvas.width - joystickRadius * 1.5,
                y: canvas.height - joystickRadius * 1.5,
                radius: joystickRadius,
                active: false,
                knobX: canvas.width - joystickRadius * 1.5,
                knobY: canvas.height - joystickRadius * 1.5,
                normalizedX: 0.5,
                normalizedY: 0,
            };

            armToggle = {
                x: canvas.width / 2 - toggleWidth / 2,
                y: toggleHeight * 5,
                width: toggleWidth,
                height: toggleHeight,
                armed: false
            };

            modeToggle = {
                x: canvas.width / 2 - toggleWidth / 2,
                y: toggleHeight * 0.5,
                width: toggleWidth,
                height: toggleHeight * 3,
                mode: 0
            };
        }

        resizeCanvas();
        window.addEventListener('resize', resizeCanvas);

        // Touch handling
        let activeTouches = {};

        canvas.addEventListener('touchstart', handleTouchStart);
        canvas.addEventListener('touchmove', handleTouchMove);
        canvas.addEventListener('touchend', handleTouchEnd);
        canvas.addEventListener('mousedown', handleMouseDown);
        canvas.addEventListener('mousemove', handleMouseMove);
        canvas.addEventListener('mouseup', handleMouseUp);

        function handleTouchStart(e) {
            e.preventDefault();
            const touches = e.changedTouches;
            for (let i = 0; i < touches.length; i++) {
                const touch = touches[i];
                activeTouches[touch.identifier] = { x: touch.pageX, y: touch.pageY };
                handleInputStart(touch.pageX, touch.pageY, touch.identifier);
            }
        }

        function handleTouchMove(e) {

            e.preventDefault();
            const touches = e.changedTouches;
            for (let i = 0; i < touches.length; i++) {
                const touch = touches[i];
                if (activeTouches[touch.identifier]) {
                    activeTouches[touch.identifier] = { x: touch.pageX, y: touch.pageY };
                    handleInputMove(touch.pageX, touch.pageY, touch.identifier);
                }
            }
        }

        function handleTouchEnd(e) {
            e.preventDefault();
            const touches = e.changedTouches;
            for (let i = 0; i < touches.length; i++) {
                const touch = touches[i];
                handleInputEnd(touch.identifier);
                delete activeTouches[touch.identifier];
            }
        }

        function handleMouseDown(e) {

            activeTouches.mouse = { x: e.pageX, y: e.pageY };
            handleInputStart(e.pageX, e.pageY, 'mouse');
        }

        function handleMouseMove(e) {
            if (activeTouches.mouse) {
                activeTouches.mouse = { x: e.pageX, y: e.pageY };
                handleInputMove(e.pageX, e.pageY, 'mouse');
            }
        }

        function handleMouseUp() {
            handleInputEnd('mouse');
            delete activeTouches.mouse;
        }

        function handleInputStart(x, y, id) {
            if (isInside(x, y, armToggle)) {
                armToggle.armed = !armToggle.armed;
            } else if (isInside(x, y, modeToggle)) {
                const height = modeToggle.height / 3;
                const clickedMode = Math.floor((y - modeToggle.y) / height);
                if (clickedMode >= 0 && clickedMode < 3) {
                    modeToggle.mode = clickedMode;
                }
            } else if (isInsideJoystickArea(x, y, leftJoystick)) {
                // console.log('left')
                leftJoystick.active = true;
                leftJoystick.activeId = id;
            } else if (isInsideJoystickArea(x, y, rightJoystick)) {
                // console.log('right')
                rightJoystick.active = true;
                rightJoystick.activeId = id;
            }
        }

        function handleInputMove(x, y, id) {
            if (leftJoystick.active && leftJoystick.activeId === id) {
                moveJoystick(leftJoystick, x, y);
            } else if (rightJoystick.active && rightJoystick.activeId === id) {
                moveJoystick(rightJoystick, x, y);
            }
        }

        function handleInputEnd(id) {
            if (leftJoystick.active && leftJoystick.activeId === id) {
                leftJoystick.active = false;
                leftJoystick.knobX = leftJoystick.x;
                leftJoystick.normalizedX = (leftJoystick.knobX - (leftJoystick.x - leftJoystick.radius)) / (leftJoystick.radius * 2);
                leftJoystick.normalizedY = 1 - (leftJoystick.knobY - (leftJoystick.y - leftJoystick.radius)) / (leftJoystick.radius * 2);
                // leftJoystick.knobY = leftJoystick.y;
            } else if (rightJoystick.active && rightJoystick.activeId === id) {
                rightJoystick.active = false;
                rightJoystick.knobX = rightJoystick.x;
                rightJoystick.knobY = rightJoystick.y;
                rightJoystick.normalizedX = (rightJoystick.knobX - (rightJoystick.x - rightJoystick.radius)) / (rightJoystick.radius * 2);
                rightJoystick.normalizedY = 1 - (rightJoystick.knobY - (rightJoystick.y - rightJoystick.radius)) / (rightJoystick.radius * 2);
            }
        }

        function isInside(x, y, rect) {
            return x > rect.x && x < rect.x + rect.width && y > rect.y && y < rect.y + rect.height;
        }

        function isInsideJoystickArea(x, y, joystick) {
            return x >= joystick.knobX - joystick.radius * 0.4 &&
                x <= joystick.knobX + joystick.radius * 0.4 &&
                y >= joystick.knobY - joystick.radius * 0.4 &&
                y <= joystick.knobY + joystick.radius * 0.4;
        }

        function isInsideCircle(x, y, circle) {
            const dx = x - circle.x;
            const dy = y - circle.y;
            return dx * dx + dy * dy <= circle.radius * circle.radius;
        }

        function moveJoystick(joystick, x, y) {
            const dx = x - joystick.x;
            const dy = y - joystick.y;
            const distance = Math.sqrt(dx * dx + dy * dy);
            const angle = Math.atan2(dy, dx);

            if (distance <= joystick.radius) {
                joystick.knobX = x;
                joystick.knobY = y;
            } else {
                joystick.knobX = joystick.x + Math.cos(angle) * joystick.radius;
                joystick.knobY = joystick.y + Math.sin(angle) * joystick.radius;
            }

            joystick.normalizedX = (joystick.knobX - (joystick.x - joystick.radius)) / (joystick.radius * 2);
            joystick.normalizedY = 1 - (joystick.knobY - (joystick.y - joystick.radius)) / (joystick.radius * 2);

            // Ensure values are within 0-1 range
            joystick.normalizedX = Math.max(0, Math.min(1, joystick.normalizedX));
            joystick.normalizedY = Math.max(0, Math.min(1, joystick.normalizedY));
        }

        // Main loop
        function update() {
            ctx.clearRect(0, 0, canvas.width, canvas.height);

            drawJoystick(leftJoystick);
            drawJoystick(rightJoystick);
            drawToggle(armToggle, armToggle.armed ? 'Armed' : 'Disarmed');
            drawModeToggle();

            requestAnimationFrame(update);
        }

        function drawJoystick(joystick) {
            ctx.beginPath();
            ctx.arc(joystick.x, joystick.y, joystick.radius, 0, Math.PI * 2);
            ctx.strokeStyle = 'white';
            ctx.lineWidth = joystick.radius * 0.05;
            ctx.stroke();

            ctx.beginPath();
            ctx.arc(joystick.knobX, joystick.knobY, joystick.radius * 0.4, 0, Math.PI * 2);
            ctx.fillStyle = 'white';
            ctx.fill();
        }

        function drawToggle(toggle, label) {
            ctx.fillStyle = toggle.armed ? 'green' : 'red';
            ctx.fillRect(toggle.x, toggle.y, toggle.width, toggle.height);

            ctx.fillStyle = 'white';
            ctx.font = `${toggle.height * 0.4}px Arial`;
            ctx.textAlign = 'center';
            ctx.textBaseline = 'middle';
            ctx.fillText(label, toggle.x + toggle.width / 2, toggle.y + toggle.height / 2);
        }

        function drawModeToggle() {
            const labels = ['Mode 1', 'Mode 2', 'Mode 3'];
            const height = modeToggle.height / 3;

            for (let i = 0; i < 3; i++) {
                ctx.fillStyle = i === modeToggle.mode ? 'green' : 'gray';
                ctx.fillRect(modeToggle.x, modeToggle.y + i * height, modeToggle.width, height);

                ctx.fillStyle = 'white';
                ctx.font = `${height * 0.4}px Arial`;
                ctx.textAlign = 'center';
                ctx.textBaseline = 'middle';
                ctx.fillText(labels[i], modeToggle.x + modeToggle.width / 2, modeToggle.y + i * height + height / 2);
            }
        }

        // Start the main loop
        update();
    </script>
</body>

</html>

)rawliteral";
