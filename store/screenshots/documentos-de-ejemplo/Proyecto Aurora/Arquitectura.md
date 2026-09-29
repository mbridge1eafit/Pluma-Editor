# Arquitectura del backend

El backend expone una API REST y delega la autenticación en un servicio de identidad.

## Crear una reserva

```mermaid
sequenceDiagram
    participant App
    participant API
    participant Auth as Identidad
    participant DB as Base de datos
    App->>API: POST /reservas
    API->>Auth: Validar token
    Auth-->>API: Token válido
    API->>DB: Guardar reserva
    DB-->>API: OK
    API-->>App: 201 Reserva creada
```

## Petición de ejemplo

```json
{
  "espacio": "sala-3",
  "inicio": "2026-10-02T09:00",
  "duracionMinutos": 120
}
```

## Decisiones

1. **PostgreSQL** como base de datos principal.
2. Colas con *RabbitMQ* para las notificaciones.
3. Despliegue en contenedores, un servicio por dominio.

> Cada decisión se documenta con su contexto y las alternativas descartadas.
