-- dzVents: upload Sofar data from Domoticz to PVOutput every 5 minutes.
-- Paste into Domoticz: Setup -> More Options -> Events -> dzVents -> Minimal.
-- Same values as gw2pvo in InverterData.py: v1 energy today, v2 power, v6 voltage.

local POWER = 'Sofar energy'		-- Electric (Instant+Counter), idx_energy
local TODAY = 'Sofar today'		-- Custom sensor, Wh, idx_today_energy
local VOLTAGE = 'Sofar L1 voltage'	-- Voltage, idx_l1_voltage
local SYSTEM_ID = ''
local API_KEY = ''

return {
	on = {
		timer = { 'every 5 minutes' },
		httpResponses = { 'pvoutput' },
	},
	logging = { marker = 'pvoutput' },
	execute = function(dz, item)
		if item.isHTTPResponse then
			if not item.ok then
				dz.log('PVOutput: ' .. item.statusCode .. ' ' .. tostring(item.data), dz.LOG_ERROR)
			end
			return
		end

		local power = dz.devices(POWER)
		-- sofar-reader sends nothing while the inverter is off at night
		if power.lastUpdate.minutesAgo > 10 then
			return
		end

		dz.openURL({
			url = 'https://pvoutput.org/service/r2/addstatus.jsp',
			method = 'POST',
			headers = {
				['X-Pvoutput-Apikey'] = API_KEY,
				['X-Pvoutput-SystemId'] = SYSTEM_ID,
				['Content-Type'] = 'application/x-www-form-urlencoded',
			},
			postData = string.format('d=%s&t=%s&v1=%d&v2=%d&v6=%.1f',
				os.date('%Y%m%d'), os.date('%H:%M'),
				dz.devices(TODAY).sensorValue, power.actualWatt,
				dz.devices(VOLTAGE).voltage),
			callback = 'pvoutput',
		})
	end
}
