#include <WiFiManager.h>

void addCustomWiFiManagerUI(WiFiManager &wm) {
  static const char jsCode[] PROGMEM = R"=====( 
<script>
window.addEventListener('load', () => {
  const msg = document.querySelector('.msg');
  console.log(msg);
  console.log(msg && msg.innerHTML.includes('Saving Credentials'));

  if (msg && msg.innerHTML.includes('Saving Credentials')) {
    document.body.style = `
      text-align:center;
      font-family:sans-serif;
      padding:20px;
      color:#222;
    `;

    msg.innerHTML = `
      <h1>🚗 Подключаемся к Wi-Fi...</h1>
      <p>⚠️ Убедись, что пароль к Wi-Fi введён правильно.</p>
      <p>⚠️ Если подключение успешно — точка <b>WiFi Car</b> исчезнет.</p>

      <h3 style="margin-top:20px;">📡 Подключись к той же сети, что и машинка</h3>
      <h3>⚠️ Если вайфай <b>WiFi Car</b> не исчез попробуй подключится снова</h3>

      <p id="countdown">Попытка автоперехода через <b>30</b> секунд</p>

      <button id="goNow"
        style="margin-top:15px;padding:10px 20px;
               font-size:16px;border:none;border-radius:8px;
               background:#007bff;color:#fff;cursor:pointer;">
        WiFi Car пропал, я уже подключился 🔗
      </button>
    `;

    let timeLeft = 30;
    const countdownEl = document.getElementById('countdown');
    const interval = setInterval(() => {
      timeLeft--;
      countdownEl.innerHTML = `Попытка автоперехода через <b>${timeLeft}</b> секунд.`;
      if (timeLeft <= 0) {
        clearInterval(interval);
        window.location.href = `${window.location.protocol}//${window.location.hostname}:82`;
      }
    }, 1000);

    document.getElementById('goNow').addEventListener('click', () => {
      clearInterval(interval);
      window.location.href = `${window.location.protocol}//${window.location.hostname}:82`;
    });
  }
});
</script>
)=====";

  wm.setCustomHeadElement(jsCode);
}
