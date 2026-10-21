import { faClose, faFilter, faMagnifyingGlass, faRotateLeft } from "@fortawesome/free-solid-svg-icons";
import { FontAwesomeIcon } from "@fortawesome/react-fontawesome";
import { t } from "i18next";
import { type ReactNode, useMemo, useState } from "react";
import type { useTable } from "../../hooks/useTable.js";
import Button from "../Button.js";
import DebouncedInput from "../form-fields/DebouncedInput.js";
import TableFiltersDrawer from "./TableFiltersDrawer.js";

export interface TableSearchProps<T> extends ReturnType<typeof useTable<T>> {
    actions?: ReactNode;
}

export default function TableSearch<T>({ table, resetFilters, globalFilter, columnFilters, actions }: TableSearchProps<T>) {
    const [drawerOpen, setDrawerOpen] = useState(false);

    const activeFiltersCount = useMemo(() => {
        let count = 0;

        for (const cf of columnFilters) {
            if (cf.value != null && cf.value !== "") {
                count++;
            }
        }

        return count;
    }, [columnFilters]);

    return (
        <div className="flex flex-row items-center gap-x-2 gap-y-1 text-sm flex-1 min-w-0">
            <div className="join flex-shrink-0">
                <label className="input input-sm join-item w-28 sm:w-auto sm:min-w-48 lg:min-w-64">
                    <FontAwesomeIcon icon={faMagnifyingGlass} />
                    <DebouncedInput onChange={table.setGlobalFilter} placeholder={t(($) => $.search)} value={globalFilter} />
                </label>
                <Button
                    item=""
                    onClick={table.setGlobalFilter}
                    className="btn btn-sm btn-square btn-warning btn-outline join-item tooltip-bottom"
                    title={t(($) => $.clear)}
                    disabled={globalFilter == null || globalFilter === ""}
                >
                    <FontAwesomeIcon icon={faClose} />
                </Button>
            </div>
            <div className="drawer drawer-end w-auto flex-shrink-0">
                <input id="table-filters-drawer" type="checkbox" className="drawer-toggle" />
                <div className="drawer-content">
                    <div className="join">
                        <label
                            htmlFor="table-filters-drawer"
                            className="drawer-button btn btn-sm btn-info btn-outline join-item tooltip tooltip-bottom"
                            onClick={() => setDrawerOpen(!drawerOpen)}
                            data-tip={t(($) => $.advanced_search)}
                        >
                            <FontAwesomeIcon icon={faFilter} />
                            <span className="hidden sm:inline ms-1">{t(($) => $.advanced_search)}</span>
                            {activeFiltersCount > 0 ? <span className="badge badge-info badge-xs ms-1">{activeFiltersCount}</span> : null}
                        </label>
                        <Button<void>
                            className="btn btn-sm btn-square btn-warning btn-outline join-item tooltip-bottom"
                            title={t(($) => $.reset)}
                            onClick={() => {
                                resetFilters();
                            }}
                            disabled={activeFiltersCount === 0}
                        >
                            <FontAwesomeIcon icon={faRotateLeft} />
                        </Button>
                    </div>
                </div>
                <div className="drawer-side z-50">
                    <label
                        htmlFor="table-filters-drawer"
                        aria-label="close sidebar"
                        className="drawer-overlay"
                        onClick={() => setDrawerOpen(false)}
                    />
                    <aside className="bg-base-100 min-h-screen w-80">
                        {drawerOpen && <TableFiltersDrawer columns={table.getAllLeafColumns()} resetFilters={resetFilters} />}
                    </aside>
                </div>
            </div>
            <div className="hidden sm:block">
                <span className="label">
                    {t(($) => $.entries)}: {table.getRowModel().rows.length}
                </span>
            </div>
            {actions}
        </div>
    );
}
