-- Not in migrations_applied.toml — must be ignored when --migrations-applied is set.
CREATE TABLE billing (
  id SERIAL PRIMARY KEY,
  secret_value TEXT NOT NULL
);
