# ecowattsense_server/api/contract.py
from pathlib import Path
from typing import Annotated

import yaml
from fastapi import APIRouter, Depends, HTTPException
from fastapi.openapi.docs import get_swagger_ui_html
from fastapi.responses import FileResponse, HTMLResponse

from ..config import settings

# Not in the server's own OpenAPI schema, so /api/docs stays about the implemented API.
router = APIRouter(prefix="/api", tags=["contract"], include_in_schema=False)


def existing_contract_file() -> Path:
    path = settings.contract_file
    if not path.is_file():
        raise HTTPException(status_code=404, detail="Contract file not found")
    return path


@router.get("/contract")
def contract_page() -> HTMLResponse:
    return get_swagger_ui_html(
        openapi_url="/api/contract/openapi.json",
        title="EcoWattSense API contract",
    )


@router.get("/contract/openapi.json")
def contract_json(path: Annotated[Path, Depends(existing_contract_file)]) -> dict:
    return yaml.safe_load(path.read_text(encoding="utf-8"))


@router.get("/contract/openapi.yaml")
def contract_yaml(path: Annotated[Path, Depends(existing_contract_file)]) -> FileResponse:
    return FileResponse(path, media_type="application/yaml")
