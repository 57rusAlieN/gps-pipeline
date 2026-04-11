# Copilot Instructions

## Рекомендации по проекту
- В проекте gps-pipeline (F:\_projects\GalileoSky\gps-pipeline) cmake/build/ctest нужно запускать через Developer Command Prompt: cmd /c "call ""C:\Program Files\Microsoft Visual Studio\18\Community\VC\Auxiliary\Build\vcvars64.bat"" && cd /d <root> && <cmake-command>". Обычный PowerShell находит x86 cl.exe вместо x64.
- В проекте gps-pipeline обновляй docs/workflow_context.md при каждом изменении в исходниках или документации.
- В проекте gps-pipeline: при TDD-цикле напоминать пользователю сделать коммит на стадии RED (тест написан, компиляция/тест не проходит) и формулировать commit message. Делать git commit самостоятельно.