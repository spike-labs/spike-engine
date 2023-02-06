## 插件系统设计

* 项目使用一个插件`配置文件`来管理所有引用的插件包。
* 插件保存放在项目工程下，但在文件系统内不可见。
* 插件之间可以有依赖关系：插件的加载存在先后顺序。
* 插件可以是脚本或者C++原生库。
* 插件可以是编辑器工具或者运行时功能。
* 打包资源时，运行时插件的资源和脚本要能够被正确打包，动态库能正确导出。
* 插件可启用/禁用：禁用的插件打包时会被忽略。
* 插件是从来源处把内容复制到插件专用目录下的：
    1. 通过指定的本地路径。
    1. 通过Git仓库。
    1. 通过插件包名从远程插件仓库下载。

---
### 插件列表定义(`配置文件`)
格式如下
```
[plugins]
com.spike.local="file://f:/path/to/plugins/plugin.cfg"
com.spike.git="https://github.spike.plugin/resp.git#tag"
com.spike.library="1.0.1"
```
以上内容定义了项目依赖三个插件，这三个插件分别来源于**本地储存**、**Git仓库**和**官方插件库**。

---
### 插件开发支持
* 提供插件模板
    1. 基于脚本语言开发的编辑器工具
    1. 基于脚本语言开发的运行时插件
    1. 基于C/C++开发的原生编辑器工具
    1. 基于C/C++开发的运行时插件

* C/C++插件编译环境预设：
    1. 至少支持平台：Windows/Android/MacOS/iOS
    1. 尽量少或者不依赖构建工具，而是通过系统的脚本(bat/shell)。


---

### 编辑器面板

* 显示插件的列表。
* 列表可以根据插件的来源折叠分类。
* 列表上的项目至少显示插件的名称、版本和启用状态。
* 选择列表上的项目，可显示更详细的插件信息。

### 官方插件库管理开发需求
* 获取插件库列表
    1. 获取插件库版本
        `https://oasis-editor.oss-cn-shanghai.aliyuncs.com/packages.spike.com/ver.txt`
    2. 下载插件描述列表
        `https://oasis-editor.oss-cn-shanghai.aliyuncs.com/packages.spike.com/list.json?v=1`

        ```json
        {
            "packages":{
                "native.module.wizard":{
                    "name":"Native Module Wizard",
                    "ver":"0.0.1",
                    "desc":"A wizard to create godot extension, etc.",
                    "spike":"4.0",
                    "author": { "name": "SEDT", "email" : "xxx@oasis.world" },
                    "date":"2022-11-03",
                    "dependencies" : {
                        "spike":"4.0",
                        "dep.pkg.com":"1.1.0"
                    }
                }
            }
        }
        ```
* 显示插件详细信息
    1. 获取插件(比如编号为`native.module.wizard`)信息版本
        `https://oasis-editor.oss-cn-shanghai.aliyuncs.com/packages.spike.com/native.module.wizard/ver.txt`
    2. 下载插件的版本信息
        `https://oasis-editor.oss-cn-shanghai.aliyuncs.com/packages.spike.com/native.module.wizard/list.json?v=1`
        ```json
        {
            "id":"native.module.wizard",
            "versions":[
                {
                    "name":"Native Module Wizard",
                    "ver":"0.0.1",
                    "desc":"A wizard to create godot extension, etc.",
                    "spike":"4.0",
                    "author": { "name": "SEDT", "email" : "xxx@oasis.world" },
                    "date":"2022-11-03",
                    "dependencies" : {
                        "spike":"4.0"
                    }
                }
            ]
        }
        ```

* 安装插件
    1. 根据插件的版本信息内的`dependencies/spike`字段判断插件是否兼容当前引擎版本。
    1. 下载指定版本到缓存路径
        `https://oasis-editor.oss-cn-shanghai.aliyuncs.com/packages.spike.com/native.module.wizard/package-0.0.1.zip`
    1. 解压插件到项目路径下的插件目录
