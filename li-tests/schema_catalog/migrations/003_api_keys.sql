-- li:censor
CREATE TABLE api_keys (
  id SERIAL PRIMARY KEY,
  token TEXT NOT NULL,
  user_id INT NOT NULL
);

ALTER TABLE users ADD COLUMN password_hash TEXT;
