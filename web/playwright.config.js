import {defineConfig} from '@playwright/test';
// Opt in when the host has no working audio output. Chrome still runs the real
// AudioContext and AudioWorklet, but this does not verify a physical device.
const launchOptions = process.env.SURGE_TEST_SILENT_OUTPUT === '1'
  ? {args:['--disable-audio-output']} : {};
export default defineConfig({testDir:'./tests',use:{channel:'chrome',headless:true,launchOptions,trace:'retain-on-failure',screenshot:'only-on-failure',baseURL:process.env.SURGE_TEST_URL || 'http://127.0.0.1:8080',viewport:{width:1100,height:750}},workers:1});
