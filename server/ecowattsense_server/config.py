# ecowattsense_server/config.py
from pathlib import Path

from pydantic import Field
from pydantic_settings import BaseSettings, SettingsConfigDict

ROOT = Path(__file__).resolve().parents[2]


class Settings(BaseSettings):
    model_config = SettingsConfigDict(
        env_file=ROOT / ".env",
        env_prefix="ECOWATTSENSE_",
        extra="ignore",
    )

    server_url: str = "http://localhost:8000"
    db_path: str = "./data/ecowattsense.db"
    client_key: str = Field(default="", validation_alias="EWS_CLIENT_KEY", repr=False)
    contract_path: str = "shared/openapi-contract.yaml"

    @property
    def db_file(self) -> Path:
        path = Path(self.db_path)
        return path if path.is_absolute() else (ROOT / path).resolve()

    @property
    def contract_file(self) -> Path:
        path = Path(self.contract_path)
        return path if path.is_absolute() else (ROOT / path).resolve()

    @property
    def database_url(self) -> str:
        return f"sqlite:///{self.db_file.as_posix()}"


settings = Settings()