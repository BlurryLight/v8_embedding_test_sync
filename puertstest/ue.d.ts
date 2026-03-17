declare module "cpp" {
    import * as UE from "ue"
    import * as cpp from "cpp"
    import {$Ref, $Nullable, cstring} from "puerts"

    class TestClass {
        constructor(p0: number);
        X: number;
        static Print(p0: string): void;
        static GetCreatedCount(): number;
        static AddAll(p0: number, p1: number, p2: number): number;
        Add(p0: number, p1: number): number;
        SetX(p0: number): number;
        Scale(p0: number): number;
        Describe(): string;
    }

}
