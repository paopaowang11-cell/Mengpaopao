// 本地测试配置：小程序直接调用华为云 IoTDA REST API。
// 仅用于电脑端微信开发者工具联调，请勿把临时 Token 提交或发布。
export const CLOUD_CONFIG = {
	// 例如：https://iotda.cn-north-4.myhuaweicloud.com
	endpoint: 'https://038a33537f.st1.iotda-app.cn-east-3.myhuaweicloud.com',
	// 华为云 IAM 项目 ID，不是项目名称
	projectId: '019e61b85cf57ce39d924a0022346239',
	// IoTDA 设备 ID，不是产品 ID
	deviceId: '6ae2ec55e094d615927e08d0_lock1',
	// IAM 临时 X-Subject-Token，过期后请重新获取
	token: 'MIIPAgYJKoZIhvcNAQcCoIIO8zCCDu8CAQExDTALBglghkgBZQMEAgEwgg0UBgkqhkiG9w0BBwGggg0FBIINAXsidG9rZW4iOnsiZXhwaXJlc19hdCI6IjIwMjYtMTAtMzFUMTQ6NDg6MDEuMDQ4MDAwWiIsInNpZ25hdHVyZSI6IkVBbGpiaTFsWVhOMExUTUFBQUFBQUFBRmpxUTNNenU2YnEvRHBHUGhmNlVaM1NUVXdDbXVPZExzRUIrV2VHZnVvdU5mb2VMTm5TT1B0b2ZqN1M3RlZlNTUwQlhoWnFQc21pbWw0N3JXSlpwcEhjMkVTR25iZjErY2VrY0krN2l4eWFTbG13YXZBN0hKVEJFUU5KYXh6V2sya1RwV3paVEd6NlAzalRoS3VsL0xMSVk2NDRldXpXOUVUWis5SFZYREdpbEdaZFBvZzN2dGxaWmh2V29jbkFrSDRZaGVmSTRBSGlSTXNaSTRnK2YzUjJGekNxWlluQy9sdElFQzh5L21sVlppc3g5ZElCMHpTdTZqM2ZRSHhNRTAzOWFVYmpQVHM2VVJmMmV5WS93cEFuRHdFQWRHd2FPOC9TTDhqUmFEZ3hhOVQ5bFd1TjdTdDFubDRIOTl3N2gyeWh6dnZEQVl2WGk2S3NFcmw3V0ZnT1U9IiwibWV0aG9kcyI6WyJwYXNzd29yZCJdLCJjYXRhbG9nIjpbXSwicm9sZXMiOlt7Im5hbWUiOiJ0ZV9hZG1pbiIsImlkIjoiMCJ9LHsibmFtZSI6InRlX2FnZW5jeSIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2NzYnNfcmVwX2FjY2VsZXJhdGlvbiIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2Vjc19kaXNrQWNjIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfZHNzX21vbnRoIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfb2JzX2RlZXBfYXJjaGl2ZSIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FfY24tc291dGgtNGMiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9kZWNfbW9udGhfdXNlciIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2Nicl9zZWxsb3V0IiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfZWNzX29sZF9yZW91cmNlIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfZXZzX1JveWFsdHkiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF93ZWxpbmticmlkZ2VfZW5kcG9pbnRfYnV5IiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfY2JyX2ZpbGUiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9ldnNfRVNpbmdsZV9jb3B5U1NEIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfZG1zLWthZmthMyIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX29ic19kZWNfbW9udGgiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9jc2JzX3Jlc3RvcmUiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9jYnJfdm13YXJlIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfaWRtZV9tYm1fZm91bmRhdGlvbiIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FfY24tZWFzdC00ZCIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX211bHRpX2JpbmQiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9ldnNfc3NkX2VudHJ5IiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfc21uX2NhbGxub3RpZnkiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9hX2FwLXNvdXRoZWFzdC0zZCIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2NzYnNfcHJvZ3Jlc3NiYXIiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9jZXNfcmVzb3VyY2Vncm91cF90YWciLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9ldnNfcmV0eXBlIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfa29vbWFwIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfZG1zLWFtcXAtYmFzaWMiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9ldnNfcG9vbF9jYSIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FfY24tc291dGh3ZXN0LTJiIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfaHdjcGgiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9lY3Nfb2ZmbGluZV9kaXNrXzQiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9zbW5fd2VsaW5rcmVkIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfaHZfdmVuZG9yIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfYV9jbi1ub3J0aC00ZSIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FfY24tbm9ydGgtNGQiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9lY3NfaGVjc194IiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfY2JyX2ZpbGVzX2JhY2t1cCIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2Vjc19hYzciLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9lcHMiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9jc2JzX3Jlc3RvcmVfYWxsIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfYV9jbi1ub3J0aC00ZiIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX29wX2dhdGVkX3JvdW5kdGFibGUiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9ldnNfZXh0IiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfcGZzX2RlZXBfYXJjaGl2ZSIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FfYXAtc291dGhlYXN0LTFlIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfYV9ydS1tb3Njb3ctMWIiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9hX2FwLXNvdXRoZWFzdC0xZCIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FwcHN0YWdlIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfYV9hcC1zb3V0aGVhc3QtMWYiLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9zbW5fYXBwbGljYXRpb24iLCJpZCI6IjAifSx7Im5hbWUiOiJvcF9nYXRlZF9ldnNfY29sZCIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2Vjc19ncHVfZzVyIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfb3BfZ2F0ZWRfbWVzc2FnZW92ZXI1ZyIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2Vjc19yaSIsImlkIjoiMCJ9LHsibmFtZSI6Im9wX2dhdGVkX2FfcnUtbm9ydGh3ZXN0LTJjIiwiaWQiOiIwIn0seyJuYW1lIjoib3BfZ2F0ZWRfaWVmX3BsYXRpbnVtIiwiaWQiOiIwIn1dLCJwcm9qZWN0Ijp7ImRvbWFpbiI6eyJuYW1lIjoiaGlkX2ZkX3Z6bzFvanJnczh3YyIsImlkIjoiMDE5ZTYxYjg1Y2Y1N2ZiNzkxYzMxODI0ZjcwMDE1ODgifSwibmFtZSI6ImNuLWVhc3QtMyIsImlkIjoiMDE5ZTYxYjg1Y2Y1N2NlMzlkOTI0YTAwMjIzNDYyMzkifSwiaXNzdWVkX2F0IjoiMjAyNi0xMC0zMFQxNDo0ODowMS4wNDgwMDBaIiwidXNlciI6eyJkb21haW4iOnsibmFtZSI6ImhpZF9mZF92em8xb2pyZ3M4d2MiLCJpZCI6IjAxOWU2MWI4NWNmNTdmYjc5MWMzMTgyNGY3MDAxNTg4In0sIm5hbWUiOiJpb3RkYV90ZXN0IiwicGFzc3dvcmRfZXhwaXJlc19hdCI6IiIsImlkIjoiMDFhMThkNDYxZDAyN2U2YWFkMDRkNTVjNmRhNWU4MmUifX19MYIBwTCCAb0CAQEwgZcwgYkxCzAJBgNVBAYTAkNOMRIwEAYDVQQIDAlHdWFuZ0RvbmcxETAPBgNVBAcMCFNoZW5aaGVuMS4wLAYDVQQKDCVIdWF3ZWkgU29mdHdhcmUgVGVjaG5vbG9naWVzIENvLiwgTHRkMQ4wDAYDVQQLDAVDbG91ZDETMBEGA1UEAwwKY2EuaWFtLnBraQIJANyzK10QYWoQMAsGCWCGSAFlAwQCATANBgkqhkiG9w0BAQEFAASCAQB2pRMM7FwaOVuy6egazsGdyCtGZMZuK6EuvdaX9Rh4MwwSaQXJCQn58yV8ukL5mk18LJ3MSxt00hdACbp9cOpsPpNWupElwi-MmgcS3SmEHMO+56hGkKh-NBghiqVgbzU0IgOblZ12JwB9R9Djne333jxYII5cp1aiyixescK8q4UDymkW2v7JWrBBCY8aPCcbHlgJVaXlL54O-pJMZk4XFfNUcrUkDRGyOrWmNlDFN1qCPdjpJF4N6s49JoBp3ooq3T9VKLR06nWjIP+E350ceTolObigw2xCyRNjm+JPfe45BLPMZc7FRdrh7dsXz5CZHgisa+YBqDUiTEXg7Tod',

	// 以下参数已按截图中的产品模型填好
	environmentServiceId: 'id1',
	temperatureProperty: 'temp',
	lockServiceId: 'id1',
	lockProperty: 'lock_state',
	// 设备上报 true 时，页面显示“门已上锁”
	lockedValue: true,
	commands: {
		unlock: {
			name: 'set_lock_state',
			paras: { lock_state: false }
		},
		lock: {
			name: 'set_lock_state',
			paras: { lock_state: true }
		}
	}
}

function clean(value) {
	return String(value || '').trim()
}

export function isCloudConfigured() {
	return Boolean(
		clean(CLOUD_CONFIG.endpoint) &&
		clean(CLOUD_CONFIG.projectId) &&
		clean(CLOUD_CONFIG.deviceId) &&
		clean(CLOUD_CONFIG.token)
	)
}

function requestCloud(path, method, data) {
	if (!isCloudConfigured()) {
		return Promise.reject(new Error('请先填写 utils/huaweiCloud.js 中的华为云配置'))
	}

	const endpoint = clean(CLOUD_CONFIG.endpoint).replace(/\/+$/, '')
	const executeRequest = () => new Promise((resolve, reject) => {
		uni.request({
			url: `${endpoint}${path}`,
			method,
			data,
			timeout: 30000,
			header: {
				'Content-Type': 'application/json',
				'X-Auth-Token': clean(CLOUD_CONFIG.token)
			},
			success(response) {
				if (response.statusCode >= 200 && response.statusCode < 300) {
					resolve(response.data || {})
					return
				}

				const body = response.data || {}
				const message = body.error_msg || body.message || body.error_description || body.error_code
				reject(new Error(message || `华为云请求失败（HTTP ${response.statusCode}）`))
			},
			fail(error) {
				const requestError = new Error(error.errMsg || '网络连接失败，请检查 endpoint 和开发者工具的域名校验设置')
				requestError.networkError = true
				reject(requestError)
			}
		})
	})

	return executeRequest().catch(error => {
		if (!error.networkError) throw error
		return new Promise(resolve => setTimeout(resolve, 700)).then(executeRequest)
	})
}

function devicePath(suffix) {
	const projectId = encodeURIComponent(clean(CLOUD_CONFIG.projectId))
	const deviceId = encodeURIComponent(clean(CLOUD_CONFIG.deviceId))
	return `/v5/iot/${projectId}/devices/${deviceId}${suffix}`
}

export function getDeviceShadow() {
	return requestCloud(devicePath('/shadow'), 'GET')
}

export function sendLockCommand(action) {
	const command = CLOUD_CONFIG.commands[action]
	if (!command || !command.name) {
		return Promise.reject(new Error(`未配置 ${action} 指令`))
	}

	return requestCloud(devicePath('/commands'), 'POST', {
		service_id: CLOUD_CONFIG.lockServiceId,
		command_name: command.name,
		paras: command.paras || {}
	})
}
