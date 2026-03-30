// clang-format off
#define UIStackNodesDecl \
typedef struct UIParentNode UIParentNode;\
struct UIParentNode{UIParentNode*next;UIElement * v;}; \
typedef struct UITextSizeNode UITextSizeNode;\
struct UITextSizeNode{UITextSizeNode*next;f32 v;}; \
typedef struct UITextColorNode UITextColorNode;\
struct UITextColorNode{UITextColorNode*next;Vec4f32 v;}; \
typedef struct UIBackgroundColorNode UIBackgroundColorNode;\
struct UIBackgroundColorNode{UIBackgroundColorNode*next;Vec4f32 v;}; \
typedef struct UIBorderColorNode UIBorderColorNode;\
struct UIBorderColorNode{UIBorderColorNode*next;Vec4f32 v;}; \
typedef struct UIBorderSizeNode UIBorderSizeNode;\
struct UIBorderSizeNode{UIBorderSizeNode*next;f32 v;}; \
typedef struct UICornerRadiusNode UICornerRadiusNode;\
struct UICornerRadiusNode{UICornerRadiusNode*next;f32 v;}; \
typedef struct UIPaddingNode UIPaddingNode;\
struct UIPaddingNode{UIPaddingNode*next;Vec4f32 v;}; \
typedef struct UIWidthNode UIWidthNode;\
struct UIWidthNode{UIWidthNode*next;UISemanticSize v;}; \
typedef struct UIHeightNode UIHeightNode;\
struct UIHeightNode{UIHeightNode*next;UISemanticSize v;};

#define UIStacksDecl \
UIParentNode parentStackBottom;\
struct{UIParentNode*top;UIParentNode*free;b32 autoPop;}parentStack;\
UITextSizeNode textSizeStackBottom;\
struct{UITextSizeNode*top;UITextSizeNode*free;b32 autoPop;}textSizeStack;\
UITextColorNode textColorStackBottom;\
struct{UITextColorNode*top;UITextColorNode*free;b32 autoPop;}textColorStack;\
UIBackgroundColorNode backgroundColorStackBottom;\
struct{UIBackgroundColorNode*top;UIBackgroundColorNode*free;b32 autoPop;}backgroundColorStack;\
UIBorderColorNode borderColorStackBottom;\
struct{UIBorderColorNode*top;UIBorderColorNode*free;b32 autoPop;}borderColorStack;\
UIBorderSizeNode borderSizeStackBottom;\
struct{UIBorderSizeNode*top;UIBorderSizeNode*free;b32 autoPop;}borderSizeStack;\
UICornerRadiusNode cornerRadiusStackBottom;\
struct{UICornerRadiusNode*top;UICornerRadiusNode*free;b32 autoPop;}cornerRadiusStack;\
UIPaddingNode paddingStackBottom;\
struct{UIPaddingNode*top;UIPaddingNode*free;b32 autoPop;}paddingStack;\
UIWidthNode widthStackBottom;\
struct{UIWidthNode*top;UIWidthNode*free;b32 autoPop;}widthStack;\
UIHeightNode heightStackBottom;\
struct{UIHeightNode*top;UIHeightNode*free;b32 autoPop;}heightStack;

