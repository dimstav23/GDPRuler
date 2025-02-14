from enum import Enum

class DbType(str, Enum):
  """GDPRuler db type."""

  ROCKSDB = "rocksdb"
  REDIS = "redis"
