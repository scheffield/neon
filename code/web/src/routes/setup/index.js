import { useEffect, useState } from "preact/hooks"
import cx from '../../lib/cx'
import style from './style.css'

const api = '_api/setup'

// from: https://www.netspotapp.com/wifi-signal-strength/what-is-rssi-level.html
const rssiToQuality = (rssi) => {
  if (rssi <= -90) return 0
  if (rssi <= -70) return 1
  return 2
}

const unique = (array, propGetter) => {
  const flags = new Set();
  return array.filter(entry => {
    const prop = propGetter(entry)
    if (flags.has(prop)) {
        return false;
    }
    flags.add(prop);
    return true;
  });
}

const Setup = () => {

  const [networks, setNetworks] = useState([])
  const [ssid, setSsid] = useState('')
  const [password, setPassword] = useState('')
  const [connecting, setConnecting] = useState(false)
  const [connectingFailed, setConnectingFailed] = useState(false)
  const [connectingSuccess, setConnectingSuccess] = useState(false)
  const [staIp, setStaIp] = useState('');

  useEffect(() => {
    const refreshState = () => fetch(api)
       .then((response) => response.json())
       .then((data) => {
          if (data.networks && data.networks.length) {
            setNetworks(
              unique(data.networks
                .sort((a, b) => b.rssi - a.rssi)
              , (network) => network.ssid)
            )
          }
          // TODO: could be a dedicated request
          if (connecting && data.wifiStatus == 'NEON_PAIRING') {
            setConnecting(false)
            setConnectingFailed(true)
          } else if (connecting && data.wifiStatus == 'NEON_PAIRING_SUCCESS') {
            setConnecting(false)
            setConnectingSuccess(true)
            setStaIp(data.staIp)
          }
       })
    refreshState()
    const id = window.setInterval(refreshState, 5000)
    return () => {
      window.clearInterval(id)
    }
  }, [])

  const onSsid = e => setSsid(e.target.value)
  const onPassword = e => setPassword(e.target.value)

  const onConnect = () => {
    if (ssid.length < 8 || connectingSuccess) {
      // TODO: don't allow empty ssid
      return;
    }
    setConnecting(true)
    setConnectingFailed(false)
    fetch(api, {
      method: 'POST',
      headers: {'Content-Type': 'application/json',},
      body: JSON.stringify({ssid, password}),
    })
  }

  return (
    <div class={style.wrapper}>
      <div>Networks:</div>
      {connecting && <div>connecting...</div>}
      {connectingFailed && <div>FAILED</div>}
      {connectingSuccess && <div>SUCCESS, connect to regular wifi and go to <a href={`http://${staIp}`}></a></div>}
      <div class={style.networks}>
        {networks.map((n, i) => (
          <div  class={cx(style.grid, style.network)} key={i} onClick={() => setSsid(n.ssid)}>
            <div class={style.rssi}>
              <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512">
                {rssiToQuality(n.rssi) >= 2 && <path d="M447.72 182.11a288 288 0 00-383.44 0" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="32"/>}
                {rssiToQuality(n.rssi) >= 1 && <path d="M393.46 249.54a201.26 201.26 0 00-274.92 0" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="32"/>}
                {rssiToQuality(n.rssi) >= 0 && <path d="M332.41 310.59a115 115 0 00-152.8 0" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="32"/>}
                <path d="M256 416a32 32 0 1132-32 32 32 0 01-32 32z" fill="currentColor"/></svg>
            </div>
            <div class={style.ssid}>{n.ssid}</div>
            <div class={style.secure}>
              {n.secure > 0 && <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 512 512"><path d="M336 208v-95a80 80 0 00-160 0v95" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="32"/><rect x="96" y="208" width="320" height="272" rx="48" ry="48" fill="none" stroke="currentColor" stroke-linecap="round" stroke-linejoin="round" stroke-width="32"/></svg>}
            </div>
          </div>
        ))}
      </div>
      <div class={style.form}>
        <div class={cx(style.grid, style.formEntry)}>
          <label class={style.label} for="ssid">SSID</label>
          <input class={style.field} type="text" id="ssid" name="ssid" onInput={onSsid} value={ssid} />
        </div>
        <div class={cx(style.grid, style.formEntry)}>
          <label class={style.label} for="password">Password</label>
          <input class={style.field} type="password" id="password" name="password" onInput={onPassword} value={password} />
        </div>
        <button class={style.submit} type="button" onClick={onConnect}>Connect</button>
      </div>
    </div>
  )
}

export default Setup
