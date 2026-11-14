<template>
	<view class="page-shell">
		<view class="glow glow-one"></view>
		<view class="glow glow-two"></view>

		<view class="page-content">
			<view class="topbar">
				<view class="brand">
					<view class="brand-mark"><text>H</text><view class="brand-dot"></view></view>
					<view>
					<text class="brand-name">HOME / SECURE</text>
					<text class="brand-caption">SMART LIVING</text>
				</view>
				</view>
				<view class="profile-button" @tap="logout"><text>林</text></view>
			</view>

			<view class="intro">
				<text class="eyebrow">{{ greeting }} · {{ weekday }}</text>
				<text class="headline">回家了，<text class="headline-light">一切刚刚好。</text></text>
				<text class="subheading">你的家，正在安心守候。</text>
			</view>

			<view v-if="isDemo" class="demo-note">
				<view class="demo-indicator"></view>
				<text>演示模式 · 请填写 utils/huaweiCloud.js 中的 4 个云端参数</text>
			</view>

			<view v-if="cloudError" class="cloud-error">
				<text class="cloud-error-title">云端读取失败</text>
				<text class="cloud-error-text">{{ cloudError }}</text>
			</view>

			<view class="lock-card">
				<view class="card-orbit orbit-a"></view>
				<view class="card-orbit orbit-b"></view>
				<view class="lock-card-top">
					<view>
						<text class="device-label">ENTRY · 01</text>
						<text class="device-name">玄关智能门锁</text>
					</view>
					<view class="online-pill"><view class="online-dot"></view><text>{{ isDemo ? '本地演示' : '云端模式' }}</text></view>
				</view>

				<view class="lock-state">
					<text class="state-title">{{ locked ? '门已上锁' : '门已解锁' }}</text>
					<text class="state-detail">{{ actionMessage || (locked ? '家门已妥善守护' : '记得随手关好家门') }}</text>
				</view>

				<view class="lock-action-wrap">
					<view class="action-halo"></view>
					<button class="lock-action" :class="{ unlocked: !locked, busy: busy }" :disabled="busy" @tap="handleLockTap">
						<view class="lock-glyph" :class="{ open: !locked }"><view class="lock-shackle"></view><view class="lock-body"><view class="lock-keyhole"></view></view></view>
						<text class="action-label">{{ busy ? '正在连接…' : (locked ? '点击开锁' : '点击上锁') }}</text>
					</button>
				</view>
				<text class="security-tip">{{ isDemo ? '演示操作只会改变页面状态' : '操作将通过华为云下发至设备' }}</text>
			</view>

			<view class="section-heading">
				<view><text class="section-title">家中环境</text><text class="section-caption">LIVE ENVIRONMENT</text></view>
				<view class="refresh-button" :class="{ refreshing: refreshing }" @tap="refreshData"><text class="refresh-icon">↻</text><text>{{ refreshing ? '更新中' : '刷新' }}</text></view>
			</view>

			<view class="environment-card">
				<view class="temperature-visual"><view class="temperature-ring"><view class="temperature-core"><text class="thermometer">°</text></view></view></view>
				<view class="temperature-copy">
					<text class="metric-label">室内温度</text>
					<view class="metric-reading"><text>{{ temperature }}</text><text class="metric-unit">°C</text></view>
					<view class="comfort"><view class="comfort-dot"></view><text>{{ temperatureLabel }}</text></view>
				</view>
				<view class="metric-divider"></view>
				<view class="sensor-info"><text class="sensor-title">温度传感器</text><text class="sensor-status">{{ isDemo ? '等待云端接入' : (hasCloudData ? '数据已同步' : '等待首次读取') }}</text><text class="sync-time">{{ updatedAt }}</text></view>
			</view>

			<view class="bottom-message"><text class="sparkle">✳</text><text>让科技守护日常，让回家成为一种期待。</text></view>
		</view>
	</view>
</template>

<script>
	import { getDeviceShadow, sendLockCommand, isCloudConfigured, CLOUD_CONFIG } from '@/utils/huaweiCloud.js'

	export default {
		data() {
			return {
				locked: true,
				temperature: '24.6',
				refreshing: false,
				busy: false,
				hasCloudData: false,
				actionMessage: '',
				cloudError: '',
				updatedAt: '示例数据',
				isDemo: !isCloudConfigured(),
				greeting: '你好',
				weekday: ''
			}
		},
		computed: {
			temperatureLabel() {
				const value = Number(this.temperature)
				if (value < 18) return '有些凉，添件衣服'
				if (value > 28) return '有些热，注意通风'
				return '温度舒适，刚刚好'
			}
		},
		onLoad() {
			const hour = new Date().getHours()
			this.greeting = hour < 11 ? '早上好' : hour < 18 ? '下午好' : '晚上好'
			this.weekday = ['星期日', '星期一', '星期二', '星期三', '星期四', '星期五', '星期六'][new Date().getDay()]
		},
		onShow() {
			this.isDemo = !isCloudConfigured()
			if (!this.isDemo) this.refreshData()
		},
		methods: {
			formatTime() {
				const now = new Date()
				const pad = value => value < 10 ? `0${value}` : value
				return `${pad(now.getHours())}:${pad(now.getMinutes())} 刚刚更新`
			},
			async refreshData() {
				if (this.refreshing) return
				if (this.isDemo) {
					this.updatedAt = '示例数据'
					return
				}
				this.refreshing = true
				this.cloudError = ''
				try {
					const shadow = await getDeviceShadow()
					console.log('[IoTDA shadow]', shadow)
					const services = Array.isArray(shadow.shadow) ? shadow.shadow : (Array.isArray(shadow.services) ? shadow.services : [])
					const environment = services.find(item => item.service_id === CLOUD_CONFIG.environmentServiceId)
					const lock = services.find(item => item.service_id === CLOUD_CONFIG.lockServiceId)
					if (!environment || !lock) {
						throw new Error(`设备影子中未找到服务 ${CLOUD_CONFIG.environmentServiceId}，请检查服务 ID`)
					}
					const environmentReported = environment.reported && (environment.reported.properties || environment.reported)
					const lockReported = lock.reported && (lock.reported.properties || lock.reported)
					const temperatureValue = environmentReported && environmentReported[CLOUD_CONFIG.temperatureProperty]
					const lockValue = lockReported && lockReported[CLOUD_CONFIG.lockProperty]
					const hasTemperature = temperatureValue !== undefined && temperatureValue !== null
					const hasLockState = lockValue !== undefined && lockValue !== null
					const toBoolean = value => value === true || value === 1 || String(value).toLowerCase() === 'true' || String(value) === '1'
					if (hasTemperature) this.temperature = Number(temperatureValue).toFixed(1)
					if (hasLockState) this.locked = toBoolean(lockValue) === toBoolean(CLOUD_CONFIG.lockedValue)
					if (!hasTemperature && !hasLockState) {
						throw new Error('设备影子中没有 temp 或 lock_state，请先让设备在线并上报一次数据')
					}
					this.hasCloudData = true
					this.updatedAt = this.formatTime()
					this.actionMessage = ''
				} catch (error) {
					this.cloudError = error.message || '读取设备数据失败'
					uni.showToast({ title: this.cloudError, icon: 'none' })
				} finally {
					this.refreshing = false
				}
			},
			handleLockTap() {
				const nextAction = this.locked ? '开锁' : '上锁'
				uni.showModal({
					title: `确认${nextAction}`,
					content: this.isDemo ? `演示模式下将模拟${nextAction}，不会控制真实门锁。` : `确定要向玄关智能门锁发送${nextAction}指令吗？`,
					confirmText: `确认${nextAction}`,
					confirmColor: '#172522',
					success: async result => {
						if (!result.confirm) return
						if (this.isDemo) {
							this.locked = !this.locked
							this.actionMessage = `演示模式 · ${nextAction}成功`
							return
						}
						this.busy = true
						try {
							await sendLockCommand(this.locked ? 'unlock' : 'lock')
							this.actionMessage = `${nextAction}指令已发送，等待设备上报`
							uni.showToast({ title: '指令已发送', icon: 'success' })
							setTimeout(() => this.refreshData(), 1500)
						} catch (error) {
							this.cloudError = error.message || '指令发送失败'
							uni.showToast({ title: this.cloudError, icon: 'none' })
						} finally {
							this.busy = false
						}
					}
				})
			},
			logout() {
				uni.removeStorageSync('homeSecureUser')
				uni.reLaunch({ url: '/pages/login/login' })
			}
		}
	}
</script>

<style>
	page { min-height: 100%; background: #f4f5f0; color: #172522; }
	.page-shell { position: relative; min-height: 100vh; overflow: hidden; background: #f4f5f0; }
	.page-content { position: relative; z-index: 1; padding: calc(38rpx + env(safe-area-inset-top)) 42rpx calc(42rpx + env(safe-area-inset-bottom)); }
	.glow { position: absolute; border-radius: 50%; pointer-events: none; }
	.glow-one { width: 420rpx; height: 420rpx; top: 180rpx; right: -290rpx; background: rgba(215, 231, 189, .33); }
	.glow-two { width: 360rpx; height: 360rpx; bottom: 160rpx; left: -280rpx; background: rgba(229, 222, 197, .28); }
	.topbar, .brand, .brand-mark, .lock-card-top, .online-pill, .section-heading, .refresh-button, .environment-card, .comfort, .sensor-info { display: flex; align-items: center; }
	.topbar { justify-content: space-between; }
	.brand { gap: 18rpx; }
	.brand-mark { width: 66rpx; height: 66rpx; justify-content: center; position: relative; border-radius: 22rpx; background: #172522; color: #d5f38c; font-size: 34rpx; font-weight: 700; }
	.brand-dot { position: absolute; width: 12rpx; height: 12rpx; right: 13rpx; bottom: 13rpx; border-radius: 50%; background: #d5f38c; }
	.brand-name, .brand-caption, .eyebrow, .device-label, .section-caption { display: block; }
	.brand-name { color: #1c2925; font-size: 20rpx; letter-spacing: 2rpx; font-weight: 700; }
	.brand-caption { margin-top: 5rpx; color: #85908b; font-size: 16rpx; letter-spacing: 3rpx; }
	.profile-button { width: 68rpx; height: 68rpx; display: flex; align-items: center; justify-content: center; border: 1rpx solid #e2e6dc; border-radius: 50%; color: #33443e; font-size: 26rpx; background: rgba(255,255,255,.72); }
	.intro { margin-top: 52rpx; }
	.eyebrow { color: #77837d; font-size: 21rpx; letter-spacing: 2rpx; }
	.headline { display: block; margin-top: 16rpx; color: #172522; font-size: 45rpx; line-height: 1.4; font-weight: 700; letter-spacing: 1rpx; }
	.headline-light { color: #80937f; font-weight: 400; }
	.subheading { display: block; margin-top: 8rpx; color: #8a938e; font-size: 23rpx; }
	.demo-note { display: flex; align-items: center; margin-top: 26rpx; padding: 18rpx 22rpx; border: 1rpx solid #e3e8d9; border-radius: 18rpx; color: #687463; background: rgba(237, 242, 226, .86); font-size: 20rpx; }
	.demo-indicator, .online-dot, .comfort-dot { width: 12rpx; height: 12rpx; flex: 0 0 auto; border-radius: 50%; }
	.demo-indicator { margin-right: 12rpx; background: #a2b779; }
	.lock-card { position: relative; overflow: hidden; margin-top: 30rpx; padding: 34rpx 34rpx 28rpx; border-radius: 34rpx; background: #172522; color: #fff; box-shadow: 0 24rpx 60rpx rgba(27, 43, 38, .16); }
	.card-orbit { position: absolute; border: 1rpx solid rgba(220, 237, 194, .08); border-radius: 50%; pointer-events: none; }
	.orbit-a { width: 500rpx; height: 500rpx; top: 80rpx; right: -220rpx; }
	.orbit-b { width: 390rpx; height: 390rpx; top: 135rpx; right: -165rpx; }
	.lock-card-top { position: relative; z-index: 1; justify-content: space-between; }
	.device-label { color: #9aa9a0; font-size: 17rpx; letter-spacing: 3rpx; }
	.device-name { display: block; margin-top: 8rpx; color: #f2f4ee; font-size: 27rpx; font-weight: 600; }
	.online-pill { gap: 10rpx; padding: 12rpx 17rpx; border: 1rpx solid rgba(227, 240, 216, .15); border-radius: 30rpx; color: #c2d3c5; font-size: 17rpx; }
	.online-dot { width: 10rpx; height: 10rpx; background: #bce583; box-shadow: 0 0 12rpx rgba(188,229,131,.5); }
	.lock-state { position: relative; z-index: 1; margin-top: 30rpx; text-align: center; }
	.state-title { display: block; color: #fff; font-size: 35rpx; font-weight: 650; letter-spacing: 2rpx; }
	.state-detail { display: block; margin-top: 8rpx; color: #9eada5; font-size: 20rpx; }
	.lock-action-wrap { position: relative; z-index: 1; width: 258rpx; height: 258rpx; margin: 20rpx auto 0; display: flex; align-items: center; justify-content: center; }
	.action-halo { position: absolute; width: 250rpx; height: 250rpx; border: 1rpx solid rgba(209, 239, 159, .12); border-radius: 50%; box-shadow: 0 0 0 18rpx rgba(209, 239, 159, .025); }
	.lock-action { position: relative; width: 208rpx; height: 208rpx; padding: 0; display: flex; flex-direction: column; align-items: center; justify-content: center; border: 0; border-radius: 50%; background: #d5f38c; color: #1b2922; box-shadow: 0 14rpx 42rpx rgba(173, 213, 107, .24), inset 0 2rpx 0 rgba(255,255,255,.5); }
	.lock-action::after { border: 0; }
	.lock-action.unlocked { background: #e6ebdc; }
	.lock-action.busy { opacity: .72; }
	.lock-glyph { position: relative; width: 58rpx; height: 62rpx; margin-bottom: 12rpx; }
	.lock-body { position: absolute; left: 7rpx; bottom: 0; width: 44rpx; height: 34rpx; border-radius: 9rpx; background: #294135; }
	.lock-shackle { position: absolute; left: 16rpx; top: 0; width: 25rpx; height: 34rpx; border: 6rpx solid #294135; border-bottom: 0; border-radius: 16rpx 16rpx 0 0; box-sizing: border-box; }
	.lock-glyph.open .lock-shackle { left: 24rpx; transform: rotate(35deg); transform-origin: left bottom; }
	.lock-keyhole { position: absolute; left: 18rpx; top: 9rpx; width: 8rpx; height: 8rpx; border-radius: 50%; background: #d5f38c; }
	.lock-keyhole::after { content: ''; position: absolute; top: 6rpx; left: 2rpx; width: 4rpx; height: 9rpx; border-radius: 4rpx; background: #d5f38c; }
	.action-label { font-size: 20rpx; font-weight: 700; }
	.security-tip { position: relative; z-index: 1; display: block; margin-top: 10rpx; text-align: center; color: #899a90; font-size: 18rpx; }
	.section-heading { justify-content: space-between; margin: 38rpx 2rpx 18rpx; }
	.section-title { display: block; color: #1d2b26; font-size: 29rpx; font-weight: 700; }
	.section-caption { margin-top: 5rpx; color: #9aa39d; font-size: 15rpx; letter-spacing: 2rpx; }
	.refresh-button { gap: 7rpx; padding: 12rpx 4rpx 12rpx 16rpx; color: #75817b; font-size: 19rpx; }
	.refresh-icon { font-size: 29rpx; line-height: 1; }
	.refreshing .refresh-icon { color: #66824b; }
	.environment-card { min-height: 188rpx; padding: 26rpx 24rpx; border: 1rpx solid rgba(222, 227, 217, .85); border-radius: 28rpx; background: rgba(255, 255, 255, .88); box-shadow: 0 12rpx 32rpx rgba(34, 48, 40, .04); }
	.temperature-visual { width: 118rpx; height: 118rpx; flex: 0 0 118rpx; display: flex; align-items: center; justify-content: center; border-radius: 50%; background: #f1f5e9; }
	.temperature-ring { width: 86rpx; height: 86rpx; display: flex; align-items: center; justify-content: center; border: 2rpx solid #cfdfb6; border-radius: 50%; }
	.temperature-core { width: 60rpx; height: 60rpx; display: flex; align-items: center; justify-content: center; border-radius: 50%; color: #738957; background: #e5edda; font-size: 46rpx; font-weight: 500; }
	.thermometer { margin-top: -8rpx; }
	.temperature-copy { min-width: 150rpx; margin-left: 22rpx; }
	.metric-label { color: #829087; font-size: 19rpx; }
	.metric-reading { margin-top: 2rpx; color: #1c2a24; font-size: 52rpx; line-height: 1.2; font-weight: 650; }
	.metric-unit { margin-left: 4rpx; color: #6f7d72; font-size: 23rpx; font-weight: 500; }
	.comfort { gap: 8rpx; margin-top: 7rpx; color: #82906f; font-size: 17rpx; white-space: nowrap; }
	.comfort-dot { width: 9rpx; height: 9rpx; background: #aec982; }
	.metric-divider { width: 1rpx; height: 102rpx; margin: 0 22rpx; background: #edf0e9; }
	.sensor-info { flex: 1; min-width: 0; flex-direction: column; align-items: flex-start; justify-content: center; }
	.sensor-title { color: #46564e; font-size: 19rpx; white-space: nowrap; }
	.sensor-status { margin-top: 9rpx; color: #9aa49c; font-size: 17rpx; white-space: nowrap; }
	.sync-time { margin-top: 8rpx; color: #b0b8b1; font-size: 15rpx; white-space: nowrap; }
	.bottom-message { display: flex; align-items: center; justify-content: center; gap: 10rpx; margin: 30rpx 0 4rpx; color: #a1aaa3; font-size: 18rpx; }
	.sparkle { color: #a6bb81; font-size: 22rpx; }

	.cloud-error { margin: 0 0 18rpx; padding: 20rpx 22rpx; border: 1rpx solid #e6c8c0; border-radius: 20rpx; background: #fff4f1; }
	.cloud-error-title, .cloud-error-text { display: block; }
	.cloud-error-title { color: #9a4e3d; font-size: 20rpx; font-weight: 700; }
	.cloud-error-text { margin-top: 8rpx; color: #9b6658; font-size: 18rpx; line-height: 1.6; word-break: break-all; }
</style>
