// ��� ����� ��� ��������������.
#define WORKSPACE_AUTOSAVE "Autosave.wrk"

// ����� ��������� ������� �����.
typedef struct WorkspaceImage
{
    char Signature[4];          // "WRK\0"

    //
    // ���������� ����������
    //

    float   Lamda;
    float   LamdaDelta;
    bool    Flipped;            // ������� "flip"

    //
    // ���� ���������
    //

    long DatabaseOffset;
    long DatabaseLength;

    //
    // ��������� �������� ��������
    //

    bool SourceImagePresent;
    long SourceImageOffset;
    long SourceImageLength;
    int ScrollX;
    int ScrollY;

    //
    // ��������� ����������� ���������
    //

    long PatternsAdded;
    long PatternsLayerOffset;

} WorkspaceImage;

void SaveWorkspace(char *filename);

void LoadWorkspace(char *filename);
