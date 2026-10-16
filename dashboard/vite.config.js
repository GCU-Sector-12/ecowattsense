import { defineConfig } from 'vite'
import react from '@vitejs/plugin-react'

// In production the server serves this build and the API on the same address.
// In development the proxy keeps the same relative URLs, so the code always calls fetch('/api/...').
// Port 8000 is the default of uvicorn (FastAPI). Change it here if the server uses another port.
export default defineConfig({
  plugins: [react()],
  server: {
    host: true,
    proxy: { '/api': process.env.API_PROXY_TARGET || 'http://localhost:8000' },
  },
  // Test settings for Vitest: a fake browser (jsdom) and a shared setup file.
  test: {
    environment: 'jsdom',
    globals: true,
    setupFiles: './src/test/setup.js',
  },
})
