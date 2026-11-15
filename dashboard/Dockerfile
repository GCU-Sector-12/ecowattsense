# Stage 1: build the static files with Vite
FROM node:22-alpine AS build
WORKDIR /app
COPY package.json package-lock.json ./
RUN npm ci
COPY . .
# Data source of the dashboard, fixed at build time. "real" calls /api/devices, /api/summary and /api/devices/{id}/history;
# empty keeps the sample data. Staging sets all three in deploy/staging.env (EWS_API-19, EWS_WEB-58).
ARG VITE_DEVICES_DATA_SOURCE=""
ARG VITE_SUMMARY_DATA_SOURCE=""
ARG VITE_HISTORY_DATA_SOURCE=""
ENV VITE_DEVICES_DATA_SOURCE=$VITE_DEVICES_DATA_SOURCE \
    VITE_SUMMARY_DATA_SOURCE=$VITE_SUMMARY_DATA_SOURCE \
    VITE_HISTORY_DATA_SOURCE=$VITE_HISTORY_DATA_SOURCE
RUN npm run build

# Stage 2: serve the built files with nginx
FROM nginx:1.27-alpine
COPY nginx.conf /etc/nginx/conf.d/default.conf
COPY --from=build /app/dist /usr/share/nginx/html
EXPOSE 80
HEALTHCHECK --interval=30s --timeout=3s --start-period=5s --retries=3 \
  CMD wget -q -O - http://127.0.0.1/healthz || exit 1
