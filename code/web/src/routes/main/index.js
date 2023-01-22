import { useEffect, useState } from 'preact/hooks'
import style from './style.css';

const api = '_api/light'

const round = (value, precision) => {
  const multiplier = Math.pow(10, precision || 0);
  return Math.round(value * multiplier) / multiplier;
}

const colorEq = (colorA, colorB) => colorA.r === colorB.r && colorA.g === colorB.g && colorA.b === colorB.b

const hueToRgb = hue => {
  const h = hue / 360;
  const s = 1; // we only care about hue
  const v = 1; // we only care about hue
  const i = Math.floor(h * 6);
  const f = h * 6 - i;
  const p = v * (1 - s);
  const q = v * (1 - f * s);
  const t = v * (1 - (1 - f) * s);
  let r;
  let g;
  let b;
  switch (i % 6) {
      case 0: r = v, g = t, b = p; break;
      case 1: r = q, g = v, b = p; break;
      case 2: r = p, g = v, b = t; break;
      case 3: r = p, g = q, b = v; break;
      case 4: r = t, g = p, b = v; break;
      case 5: r = v, g = p, b = q; break;
  }
  return {
      r: Math.round(r * 255),
      g: Math.round(g * 255),
      b: Math.round(b * 255)
  };
}

const colors = [
  hueToRgb(0),
  hueToRgb(60),
  hueToRgb(120),
  hueToRgb(180),
  hueToRgb(240),
  hueToRgb(300),
  {r: 255, g: 255, b: 255}
]

const Main = () => {
  const [brightness, setBrightness] = useState(0)
  const [charging, setCharging] = useState(false)
  const [battery, setBattery] = useState(0)
  const [batteryTemp, setBatteryTemp] = useState(0)
  const [color, setColor] = useState({r: 0, g: 0, b: 0})

  const [requesting, setRequesting] = useState(false)

  const update = (req) => {
    if (requesting) {
      return
    }
    setRequesting(true)
    return fetch(api, {
      method: 'POST',
      headers: {'Content-Type': 'application/json',},
      body: JSON.stringify(req),
    })
      .then((response) => response.json())
      .then((data) => {
        setRequesting(false)
        return data
      })
  }

  useEffect(() => {
    const refreshState = () => fetch(api)
      .then((response) => response.json())
      .then((data) => {
        setBrightness(data.brightness)
        setCharging(data.charging)
        setBattery(data.battery)
        setBatteryTemp(data.batteryTemp)
        setColor({r: data.r, g: data.g, b: data.b})
      })
    refreshState()
    const id = window.setInterval(refreshState, 5000)
    return () => {
      window.clearInterval(id)
    }
  }, [])

  const updateBrightness = (event) => {
    update({brightness: event.target.value})
      .then(data => setBrightness(data.brightness))
  }

  const switchOff = () => {
    update({brightness: 0})
      .then(data => setBrightness(data.brightness))
  }

  const updateColor = (event) => {
    const color = colors[event.target.dataset.color];
    update({r: color.r, g: color.g, b: color.b})
      .then(data => setColor({r: data.r, g: data.g, b: data.b}))
  }

  return (
    <div class={style.wrapper}>
      <div class={style.status}>
        <div class={style.battery}>
          {/* svg from https://ionic.io/ionicons (search for battery) */}
          <svg xmlns="http://www.w3.org/2000/svg" id={style.battery_icon} viewBox="0 0 512 512">
            <rect x="32" y="144" width="400" height="224" rx="45.7" ry="45.7" fill="none" stroke="currentColor" stroke-linecap="square" stroke-miterlimit="10" stroke-width="32"/>
            <rect x="85.69" y="198.93" width={290 * (Math.min(Math.max(3.7, battery), 4.2) - 3.7) / (4.2 - 3.7)} height="114.13" rx="4" ry="4" stroke="currentColor" stroke-linecap="square" stroke-miterlimit="10" stroke-width="32"/>
            {charging && <path x="200" transform="scale(.5 .5) translate(200 256)" d="M315.27 33L96 304h128l-31.51 173.23a2.36 2.36 0 002.33 2.77h0a2.36 2.36 0 001.89-.95L416 208H288l31.66-173.25a2.45 2.45 0 00-2.44-2.75h0a2.42 2.42 0 00-1.95 1z" fill="#FFE60A" stroke="#FFE60A" stroke-linecap="round" stroke-linejoin="round" stroke-width="64"/>}
            <path fill="none" stroke="currentColor" stroke-linecap="round" stroke-miterlimit="10" stroke-width="32" d="M480 218.67v74.66"/>
          </svg>
        </div>
        <div class={style.charging}>Battery: {batteryTemp > 10 ? round(batteryTemp, 1) : 'N/A'}°C</div>
      </div>
      {requesting && <div class={style.loading_indicator}>updating...</div>}
      <div class={style.control}>
        <button type="button" onClick={switchOff}>Switch off</button>
      </div>
      <div class={style.brightness_control}>
        <label for="brightness">Brightness</label>
        <input type="range" id="brightness" name="brightness" min="0" max="255" value={brightness} onChange={updateBrightness}/>
      </div>
      <div class={style.colors}>
        {colors.map((c, i) => <div 
            class={[style.color, colorEq(c, color) ? style.active : ''].join(' ')} 
            key={i} 
            style={{backgroundColor: `rgb(${c.r}, ${c.g}, ${c.b})`, filter: colorEq(c, color) ? `drop-shadow(0 0 8px rgb(${c.r}, ${c.g}, ${c.b}))` : ''}} 
            onClick={updateColor} 
            data-color={i}
          ></div>)}
      </div>
    </div>
  )
}

export default Main