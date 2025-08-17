# Surge TCP Control Protocol

## Connection
- Port: 7833 (SURGETCP)
- Host: localhost only (security)
- Format: Simple text commands with JSON responses

## Commands

### Load Preset
```
PRESET:LOAD:<preset_name>
```
Response:
```json
{"status": "ok", "preset": "DeepBass"}
```

### Set Parameter
```
PARAM:SET:<param_id>:<value>
```
Response:
```json
{"status": "ok", "param": "cutoff", "value": 0.75}
```

### Get Preset List
```
PRESET:LIST
```
Response:
```json
{"status": "ok", "presets": ["DeepBass", "WarmPad", "PluckLead"]}
```

### Health Check
```
PING
```
Response:
```json
{"status": "ok", "version": "1.0"}
```

## Error Responses
```json
{"status": "error", "message": "Preset not found: FakeName"}
```