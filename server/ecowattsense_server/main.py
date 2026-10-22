# ecowattsense_server/main.py
from contextlib import asynccontextmanager

from fastapi import FastAPI

from .api import health, reports
from .db import init_db


@asynccontextmanager
async def lifespan(app: FastAPI):
    init_db()
    yield


app = FastAPI(title="EcoWattSense server", version="0.1.0", lifespan=lifespan)
app.include_router(health.router)
app.include_router(reports.router)