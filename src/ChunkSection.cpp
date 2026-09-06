bool ChunkSection::isFaceVisible(int x, int y, int z, Direction dir) const {
    int nx = x, ny = y, nz = z;
    switch (dir) {
        case DIR_UP:    ny++; break;
        case DIR_DOWN:  ny--; break;
        case DIR_NORTH: nz--; break;
        case DIR_SOUTH: nz++; break;
        case DIR_WEST:  nx--; break;
        case DIR_EAST:  nx++; break;
    }

    // إذا خرج عن حدود الـ Chunk نعتبره مرئياً حالياً
    if (nx < 0 || nx >= SECTION_SIZE || ny < 0 || ny >= SECTION_SIZE || nz < 0 || nz >= SECTION_SIZE) {
        return true; 
    }

    BlockType current = getBlock(x, y, z);
    BlockType neighbor = getBlock(nx, ny, nz);

    // 1. إذا كان الجار هواء -> ارسم الوجه دائماً
    if (isBlockAir(neighbor)) return true;

    // 2. إذا كان الجار صلباً معتماً -> احذف الوجه دائماً (لا شيء يظهر خلف الصخر)
    if (isBlockOpaque(neighbor)) return false;

    // 3. إذا كانت البلوكة الحالية صلبة والجار شفاف (مثل صخر بجانبه زجاج أو أوراق)
    // -> ارسم وجه الصخر فوراً لأننا سنراه عبر الزجاج!
    if (isBlockOpaque(current) && !isBlockOpaque(neighbor)) return true;

    // 4. زجاج بجانب زجاج من نفس النوع:
    // -> احذف الأوجه الداخلية الملتحمة بينهما لمنع تشوه المشهد
    if (current == neighbor) return false;

    // 5. في باقي الحالات (مثل زجاج يلامس ورقة شجر) -> ارسم الوجه
    return true;
}
