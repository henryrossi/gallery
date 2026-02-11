#define UIStackNodesDecl \
typedef struct UIParentNode UIParentNode;\
struct UIParentNode{UIParentNode*next;UIElement * v;}; \
typedef struct UITextSizeNode UITextSizeNode;\
struct UITextSizeNode{UITextSizeNode*next;f32 v;}; \
typedef struct UITextColorNode UITextColorNode;\
struct UITextColorNode{UITextColorNode*next;Vec4f32 v;}; \
typedef struct UIBackgroundColorNode UIBackgroundColorNode;\
struct UIBackgroundColorNode{UIBackgroundColorNode*next;Vec4f32 v;}; \
typedef struct UIWidthNode UIWidthNode;\
struct UIWidthNode{UIWidthNode*next;UISemanticSize v;}; \
typedef struct UIHeightNode UIHeightNode;\
struct UIHeightNode{UIHeightNode*next;UISemanticSize v;};

#define UIStacksDecl \
UIParentNode parentStackBottom;\
struct{UIParentNode*top;UIParentNode*free;}parentStack;\
UITextSizeNode textSizeStackBottom;\
struct{UITextSizeNode*top;UITextSizeNode*free;}textSizeStack;\
UITextColorNode textColorStackBottom;\
struct{UITextColorNode*top;UITextColorNode*free;}textColorStack;\
UIBackgroundColorNode backgroundColorStackBottom;\
struct{UIBackgroundColorNode*top;UIBackgroundColorNode*free;}backgroundColorStack;\
UIWidthNode widthStackBottom;\
struct{UIWidthNode*top;UIWidthNode*free;}widthStack;\
UIHeightNode heightStackBottom;\
struct{UIHeightNode*top;UIHeightNode*free;}heightStack;

