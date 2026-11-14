<template>
	<view class="login-page">
		<view class="decor decor-top"></view>
		<view class="decor decor-bottom"></view>
		<view class="login-content">
			<view class="top-brand">
				<view class="brand-mark"><text>H</text><view class="brand-dot"></view></view>
				<view><text class="brand-name">HOME / SECURE</text><text class="brand-caption">SMART LIVING</text></view>
			</view>

			<view class="welcome-block">
				<view class="welcome-kicker"><view class="kicker-line"></view><text>YOUR HOME, IN YOUR HAND</text></view>
				<text class="welcome-title">把安心，<text>带回家。</text></text>
				<text class="welcome-copy">连接你的智能家居，随时感知温度，<text class="copy-break">轻轻一点，开启归家时刻。</text></text>
			</view>

			<view class="login-card">
				<view class="card-heading"><text class="card-title">欢迎回来</text><text class="card-subtitle">登录后管理你的家</text></view>
				<view class="form-item">
					<text class="field-label">手机号</text>
					<view class="field-box"><text class="field-prefix">+86</text><view class="field-divider"></view><input v-model="phone" class="field-input" type="number" maxlength="11" placeholder="请输入手机号" placeholder-class="placeholder" /></view>
				</view>
				<view class="form-item password-item">
					<view class="label-row"><text class="field-label">密码</text><text class="forgot-link" @tap="showForgotTip">忘记密码？</text></view>
					<view class="field-box"><text class="field-icon">✳</text><view class="field-divider"></view><input v-model="password" class="field-input" :password="!showPassword" placeholder="请输入密码" placeholder-class="placeholder" /><text class="eye-toggle" @tap="showPassword = !showPassword">{{ showPassword ? '隐藏' : '显示' }}</text></view>
				</view>

				<button class="login-button" :disabled="submitting" @tap="login"><text>{{ submitting ? '正在进入…' : '进入体验' }}</text><text class="button-arrow">↗</text></button>
				<view class="local-notice"><view class="notice-icon">i</view><text>本地体验登录 · 暂不校验账号与密码</text></view>
			</view>

			<view class="login-footer"><view class="footer-line"></view><text>SAFE HOME, SIMPLE LIFE</text><view class="footer-line"></view></view>
		</view>
	</view>
</template>

<script>
	export default {
		data() {
			return {
				phone: '',
				password: '',
				showPassword: false,
				submitting: false
			}
		},
		methods: {
			login() {
				if (!/^1\d{10}$/.test(this.phone)) {
					uni.showToast({ title: '请输入有效的手机号', icon: 'none' })
					return
				}
				if (!this.password.trim()) {
					uni.showToast({ title: '请输入密码', icon: 'none' })
					return
				}
				this.submitting = true
				uni.setStorageSync('homeSecureUser', { phone: this.phone })
				setTimeout(() => {
					this.submitting = false
					uni.reLaunch({ url: '/pages/index/index' })
				}, 220)
			},
			showForgotTip() {
				uni.showToast({ title: '体验版暂未接入账号服务', icon: 'none' })
			}
		}
	}
</script>

<style>
	page { min-height: 100%; background: #f4f5f0; color: #172522; }
	.login-page { position: relative; min-height: 100vh; overflow: hidden; background: #f4f5f0; }
	.login-content { position: relative; z-index: 1; padding: calc(54rpx + env(safe-area-inset-top)) 44rpx calc(34rpx + env(safe-area-inset-bottom)); }
	.decor { position: absolute; pointer-events: none; border-radius: 50%; }
	.decor-top { width: 600rpx; height: 600rpx; right: -360rpx; top: -255rpx; background: #e7eddb; }
	.decor-bottom { width: 500rpx; height: 500rpx; left: -360rpx; bottom: 60rpx; background: #ece9de; }
	.top-brand { display: flex; align-items: center; gap: 18rpx; }
	.brand-mark { position: relative; width: 68rpx; height: 68rpx; display: flex; align-items: center; justify-content: center; border-radius: 23rpx; color: #d5f38c; background: #172522; font-size: 35rpx; font-weight: 700; }
	.brand-dot { position: absolute; width: 12rpx; height: 12rpx; right: 13rpx; bottom: 13rpx; border-radius: 50%; background: #d5f38c; }
	.brand-name, .brand-caption { display: block; }
	.brand-name { color: #1c2925; font-size: 20rpx; letter-spacing: 2rpx; font-weight: 700; }
	.brand-caption { margin-top: 5rpx; color: #85908b; font-size: 16rpx; letter-spacing: 3rpx; }
	.welcome-block { margin-top: 84rpx; }
	.welcome-kicker { display: flex; align-items: center; gap: 12rpx; color: #87937f; font-size: 16rpx; letter-spacing: 2rpx; }
	.kicker-line { width: 30rpx; height: 2rpx; background: #a5b987; }
	.welcome-title { display: block; margin-top: 23rpx; color: #172522; font-size: 58rpx; line-height: 1.34; font-weight: 700; letter-spacing: 2rpx; }
	.welcome-title text { color: #82937c; font-weight: 400; }
	.welcome-copy { display: block; margin-top: 16rpx; color: #89938c; font-size: 22rpx; line-height: 1.9; }
	.copy-break { display: block; }
	.login-card { margin-top: 58rpx; padding: 38rpx 32rpx 30rpx; border: 1rpx solid rgba(225, 229, 218, .9); border-radius: 34rpx; background: rgba(255, 255, 255, .9); box-shadow: 0 22rpx 65rpx rgba(36, 49, 40, .07); }
	.card-title { display: block; color: #1d2b26; font-size: 34rpx; font-weight: 700; }
	.card-subtitle { display: block; margin-top: 7rpx; color: #98a19a; font-size: 20rpx; }
	.form-item { margin-top: 32rpx; }
	.password-item { margin-top: 25rpx; }
	.field-label { display: block; color: #536159; font-size: 20rpx; font-weight: 600; }
	.label-row { display: flex; align-items: center; justify-content: space-between; }
	.forgot-link { color: #899779; font-size: 18rpx; }
	.field-box { height: 92rpx; box-sizing: border-box; display: flex; align-items: center; margin-top: 12rpx; padding: 0 20rpx; border: 1rpx solid #e6eae2; border-radius: 18rpx; background: #fafbf8; }
	.field-prefix { color: #536159; font-size: 21rpx; }
	.field-icon { width: 37rpx; color: #90a176; font-size: 20rpx; text-align: center; }
	.field-divider { width: 1rpx; height: 34rpx; margin: 0 18rpx; background: #e0e5db; }
	.field-input { flex: 1; min-width: 0; height: 90rpx; color: #25332c; font-size: 22rpx; }
	.placeholder { color: #b4bcb5; }
	.eye-toggle { margin-left: 12rpx; color: #8f9a8e; font-size: 17rpx; }
	.login-button { height: 94rpx; display: flex; align-items: center; justify-content: center; margin-top: 36rpx; padding: 0 30rpx; border-radius: 20rpx; color: #192720; background: #d5f38c; box-shadow: 0 13rpx 25rpx rgba(168, 202, 105, .22); font-size: 23rpx; font-weight: 700; }
	.login-button::after { border: 0; }
	.login-button[disabled] { opacity: .75; }
	.button-arrow { margin-left: 15rpx; font-size: 27rpx; font-weight: 500; }
	.local-notice { display: flex; align-items: center; justify-content: center; gap: 9rpx; margin-top: 21rpx; color: #a3aaa2; font-size: 17rpx; }
	.notice-icon { width: 22rpx; height: 22rpx; border: 1rpx solid #b8c2af; border-radius: 50%; color: #98a58d; font-size: 15rpx; line-height: 21rpx; text-align: center; }
	.login-footer { display: flex; align-items: center; justify-content: center; gap: 14rpx; margin-top: 48rpx; color: #abb2aa; font-size: 14rpx; letter-spacing: 2rpx; }
	.footer-line { width: 35rpx; height: 1rpx; background: #dfe4da; }
</style>
