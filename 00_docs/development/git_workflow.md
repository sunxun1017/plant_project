# Git Workflow

## 1. Purpose

本文档定义本项目的 Git 使用规范，包括：

* 分支模型
* 分支命名
* Commit Message
* Commit 粒度
* Pull Request / Merge Request
* Rebase 与 Merge
* Tag 与版本发布
* 禁止事项

目标是保持 Git 历史：

* 清晰
* 可追踪
* 可回滚
* 易于 Code Review
* 易于长期维护

本项目采用：

```text
Trunk-Based Development
+
Short-Lived Feature Branches
+
Conventional Commits
+
Squash Merge
+
Semantic Versioning
```

---

# 2. Main Branch

项目长期主分支：

```text
main
```

`main` 必须尽可能始终满足：

```text
可编译
可测试
可运行
可发布
```

原则上不直接在 `main` 上开发功能。

正常开发流程：

```text
main
 │
 ├── feat/xxx
 ├── fix/xxx
 ├── refactor/xxx
 └── docs/xxx
        │
        ▼
       PR
        │
        ▼
      main
```

---

# 3. Branch Model

项目只维护一个主要长期分支：

```text
main
```

不默认建立：

```text
develop
```

也不采用完整 Git Flow：

```text
main
develop
release/*
hotfix/*
feature/*
```

除非未来团队和发布流程的复杂度明确需要。

功能开发使用短生命周期分支。

分支完成并合入 `main` 后应删除。

---

# 4. Branch Naming

统一格式：

```text
<type>/<short-description>
```

使用：

```text
lowercase
+
kebab-case
```

例如：

```text
feat/ble-communication
feat/plant-behavior
feat/breathing-light
feat/servo-motion

fix/ble-reconnect
fix/servo-jitter

refactor/motion-port
refactor/behavior-engine

docs/git-workflow
docs/dependency-rules

test/behavior-service

build/cmake-components

ci/host-tests
```

推荐的 Branch Type：

```text
feat/       新功能
fix/        Bug 修复
refactor/   重构
docs/       文档
test/       测试
build/      构建系统
ci/         CI/CD
perf/       性能优化
chore/      其他维护任务
```

禁止使用模糊名称：

```text
dev
test
new
update
temp
my-branch
kaqiu
fix-bug
```

避免：

```text
feat/add_ble_function
feat/添加蓝牙
```

推荐：

```text
feat/ble-communication
```

---

# 5. Commit Message

项目采用 Conventional Commits 风格。

格式：

```text
<type>(<scope>): <description>
```

例如：

```text
feat(behavior): add grow behavior

feat(motion): add servo actuator

feat(light): add breathing animation

feat(ble): add device state notification

fix(motion): prevent servo overshoot

fix(ble): handle reconnect after link loss

refactor(core): separate command and event models

docs(arch): define dependency rules

test(behavior): add grow behavior tests

build(cmake): add core library target
```

---

# 6. Commit Types

允许使用：

```text
feat
fix
refactor
docs
test
build
ci
perf
style
chore
```

语义：

```text
feat
新增用户或系统可感知的能力。

fix
修复已有功能中的错误。

refactor
修改代码结构，但不改变外部行为。

docs
只修改文档。

test
增加或修改测试。

build
修改 CMake、依赖、编译选项等。

ci
修改 CI/CD。

perf
性能优化。

style
只涉及格式、空格、代码风格，不改变逻辑。

chore
不属于其他类别的维护工作。
```

---

# 7. Commit Scope

Scope 应尽量对应架构模块。

推荐：

```text
core
ports
behavior
motion
light
haptic
comm
ble
wifi
protocol
espidf
bsp
product
test
cmake
ci
arch
git
```

例如：

```text
feat(motion): add grow motion profile

feat(haptic): add double pulse pattern

fix(light): clamp brightness output

refactor(protocol): simplify command decoder

docs(git): define repository workflow
```

---

# 8. Commit Description

Description 应：

```text
简短
明确
描述一个逻辑变化
使用英文
使用祈使语气
```

推荐：

```text
add servo actuator
```

而不是：

```text
added servo actuator
```

禁止：

```text
update
fix
modify code
change something
修改bug
final
final2
test123
```

---

# 9. Atomic Commit

一个 Commit 应表达一个完整的逻辑变化。

例如实现 Motion 功能时，可以形成：

```text
feat(ports): add motion port

feat(motion): add motion service

feat(espidf): add servo adapter

test(motion): add motion service tests
```

不要把无关内容塞进同一个 Commit：

```text
feat: add BLE, fix motor, update docs and refactor logger
```

原因是这种 Commit：

```text
难 Review
难 Revert
难定位 Bug
难理解历史
```

---

# 10. Commit Granularity

Commit 不应过大，也不应过碎。

不推荐：

```text
add file

add include

fix typo

fix compile

fix again

final fix
```

开发过程中可以产生临时 Commit：

```text
wip: test servo
```

但在合入 `main` 前应整理历史。

---

# 11. Development Workflow

开始开发前：

```bash
git switch main
git pull --ff-only
```

创建功能分支：

```bash
git switch -c feat/breathing-light
```

开发过程中检查：

```bash
git status
git diff
```

暂存：

```bash
git add <files>
```

提交前再次确认：

```bash
git diff --cached
```

然后提交：

```bash
git commit -m "feat(light): add breathing animation"
```

---

# 12. Avoid Blind git add

不建议形成以下习惯：

```bash
git add .
git commit -m "update"
```

推荐：

```bash
git status

git diff

git add 02_ports/light/
git add 03_services/lighting/

git diff --cached

git commit -m "feat(light): add breathing animation"
```

开发者必须明确知道：

> 当前 Commit 中到底包含哪些变化。

---

# 13. Rebase

功能分支开发过程中，如果 `main` 已更新，优先使用：

```bash
git fetch origin

git rebase origin/main
```

推荐历史：

```text
main ─────────────●─────────●
                   \
                    A──B──C
```

不建议反复：

```bash
git merge main
```

导致：

```text
Merge branch 'main' into feat/xxx
Merge branch 'main' into feat/xxx
```

充斥历史。

---

# 14. Public History

可以整理自己的 Feature Branch：

```bash
git rebase -i origin/main
```

但禁止随意重写公共 `main` 历史。

原则：

> 可以重写自己的历史，不要重写团队共同依赖的历史。

---

# 15. Force Push

原则上禁止：

```bash
git push --force
```

如果 Feature Branch 在 Rebase 后确实需要强推，应使用：

```bash
git push --force-with-lease
```

而不是：

```bash
git push --force
```

`--force-with-lease` 可以降低覆盖他人提交的风险。

---

# 16. Pull Request

一个 PR 应尽量对应：

```text
一个 Feature
一个 Bug Fix
一个 Refactor
一个 Architecture Change
```

例如：

```text
feat(motion): add servo actuator support
```

可以同时包含：

```text
Motion Port
Motion Service
Servo Adapter
Unit Test
必要文档
```

因为这些属于同一个完整功能。

不应该一个 PR 同时包含：

```text
BLE
Servo
OTA
Logger
Architecture Refactor
```

多个无关改动。

---

# 17. PR Requirements

合并前至少满足：

```text
Build Pass
Tests Pass
No New Warnings
Architecture Rules Pass
No Debug Code
No Secrets
Documentation Updated When Necessary
```

尤其禁止：

```text
03_services
    ↓
ESP-IDF
```

例如：

```cpp
#include "esp_wifi.h"
```

直接出现在 Service 层属于架构违规。

---

# 18. Merge Strategy

推荐：

```text
Feature Branch
      ↓
     PR
      ↓
   Review / CI
      ↓
  Squash Merge
      ↓
     main
```

例如 Feature Branch 中开发过程：

```text
feat: add servo
fix compile
fix test
adjust parameter
fix typo
```

最终合并进 `main`：

```text
feat(motion): add servo actuator support
```

这样 `main` 的历史保持清晰。

---

# 19. Main Branch History

理想的 `main`：

```text
docs(arch): define initial architecture

feat(core): add device state model

feat(behavior): add behavior service

feat(motion): add servo actuator support

feat(light): add breathing animation

feat(haptic): add vibration feedback

feat(ble): add mobile communication
```

而不是：

```text
update
fix
fix again
test
final
final2
merge main
tmp
```

Git History 是项目设计历史的一部分。

---

# 20. Architecture Changes

架构变化应该独立提交。

例如：

```text
refactor(arch): move runtime abstraction into ports
```

不要把大型架构调整隐藏在：

```text
feat(ble): add bluetooth
```

中。

如果涉及重要架构决策，还应该同时创建 ADR：

```text
00_docs/adr/
```

例如：

```text
0001-use-ports-and-adapters.md
0002-separate-behavior-from-hardware.md
0003-use-single-system-task.md
```

对应 Commit：

```text
docs(adr): record behavior hardware separation
```

---

# 21. Versioning

项目使用 Semantic Versioning：

```text
MAJOR.MINOR.PATCH
```

例如：

```text
v0.1.0
v0.2.0
v0.2.1
v1.0.0
```

语义：

```text
MAJOR
不兼容的重大变化。

MINOR
向后兼容的新功能。

PATCH
向后兼容的 Bug Fix。
```

项目早期可以保持：

```text
0.x.x
```

第一版真正稳定产品可以发布：

```text
v1.0.0
```

---

# 22. Tags

正式版本使用 Annotated Tag：

```bash
git tag -a v0.1.0 -m "Plant firmware v0.1.0"
```

推送：

```bash
git push origin v0.1.0
```

禁止使用：

```text
final
final-final
release-new
release2
```

作为正式版本管理方式。

---

# 23. Generated Files

默认不提交：

```text
build/
*.elf
*.bin
*.map
临时文件
IDE Cache
日志
```

产品真正需要进入版本控制的配置应明确维护。

例如 ESP-IDF 可以维护：

```text
sdkconfig.defaults
```

以保证产品构建具有稳定默认配置。

---

# 24. Secrets

Git 中禁止提交：

```text
Wi-Fi Password
API Token
Private Key
Certificate Private Key
Production Credential
Personal Credential
```

例如禁止：

```cpp
constexpr auto WIFI_PASSWORD = "12345678";
```

Secret 一旦 Commit，即使后续删除，仍然存在于 Git History 中。

因此：

> Secret 不应该进入 Git，而不是进入 Git 后再删除。

---

# 25. Recommended Daily Workflow

标准工作流：

```text
更新 main
   ↓
创建 Feature Branch
   ↓
开发
   ↓
Build
   ↓
Test
   ↓
Review Diff
   ↓
Commit
   ↓
Rebase main
   ↓
Push
   ↓
PR
   ↓
CI / Review
   ↓
Squash Merge
   ↓
Delete Branch
```

---

# 26. Summary

本项目 Git 核心规则：

```text
main 永远保持可用

使用短生命周期 Feature Branch

Branch:
    type/short-description

Commit:
    type(scope): description

一个 Commit 一个逻辑变化

一个 PR 一个主要目的

Feature Branch 使用 Rebase

main 使用 Squash Merge

版本使用 Semantic Versioning

重要架构变化使用 ADR

禁止 Secret 进入 Git
```

Git 不只是代码备份工具。

Git History 应能够回答：

> 系统什么时候发生了什么变化，以及为什么发生这些变化。

