# ecowattsense_server/main.py
from contextlib import asynccontextmanager

from fastapi import FastAPI

from . import models  # noqa: F401  (the import makes create_all see the tables)
from .api import auth, contract, devices, health, history, ping, settings, summary
from .db import init_db


@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    yield


# The docs live under /api, because the proxy in front of the server only forwards /api/.
app = FastAPI(
    title="EcoWattSense server",
    version="0.1.0",
    lifespan=lifespan,
    docs_url="/api/docs",
    redoc_url="/api/redoc",
    openapi_url="/api/openapi.json",
)
app.include_router(health.router)
app.include_router(contract.router)
app.include_router(auth.router)
app.include_router(settings.router)
app.include_router(ping.router)
app.include_router(devices.router)
app.include_router(summary.router)
app.include_router(history.router)
