FROM node:22-alpine AS deps
WORKDIR /app
COPY web-server/package.json web-server/package-lock.json ./
RUN npm ci

FROM alpine:3.21 AS cpp-build
RUN apk add --no-cache cmake make g++ git
WORKDIR /build
COPY includes/ ./
RUN cmake -S . -B out -DCMAKE_POLICY_VERSION_MINIMUM=3.5 -DCMAKE_BUILD_TYPE=Release \
    && cmake --build out -j"$(nproc)" --target deployer

FROM node:22-alpine
RUN apk add --no-cache libstdc++
WORKDIR /app
COPY --from=deps /app/node_modules ./node_modules
COPY web-server/package.json web-server/tsconfig.json ./
COPY web-server/src/ ./src/
COPY --from=cpp-build /build/out/deployer /opt/deployer/deployer

ENV WORKER_BIN=/opt/deployer/deployer
EXPOSE 3000

CMD ["npx", "ts-node", "src/server.ts"]
