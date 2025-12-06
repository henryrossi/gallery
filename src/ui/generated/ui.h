#define UIStackNodesDecl \
typedef struct UIParentNode UIParentNode;\
struct UIParentNode{UIParentNode*next;UIElement * v;}; \
typedef struct UITextColorNode UITextColorNode;\
struct UITextColorNode{UITextColorNode*next;Vec4 v;}; \
typedef struct UIBackgroundColorNode UIBackgroundColorNode;\
struct UIBackgroundColorNode{UIBackgroundColorNode*next;Vec4 v;}; \
typedef struct UIWidthNode UIWidthNode;\
struct UIWidthNode{UIWidthNode*next;SemanticSize v;}; \
typedef struct UIHeightNode UIHeightNode;\
struct UIHeightNode{UIHeightNode*next;SemanticSize v;}; \

#define UIStacksDecl \
UIParentNode parentStackBottom;\
struct{UIParentNode*top;UIParentNode*free;}parentStack;\
UITextColorNode textColorStackBottom;\
struct{UITextColorNode*top;UITextColorNode*free;}textColorStack;\
UIBackgroundColorNode backgroundColorStackBottom;\
struct{UIBackgroundColorNode*top;UIBackgroundColorNode*free;}backgroundColorStack;\
UIWidthNode widthStackBottom;\
struct{UIWidthNode*top;UIWidthNode*free;}widthStack;\
UIHeightNode heightStackBottom;\
struct{UIHeightNode*top;UIHeightNode*free;}heightStack;\

